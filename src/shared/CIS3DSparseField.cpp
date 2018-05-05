#include "CIS3DSparseField.h"
#include <stdexcept>
#include <stdio.h>
#include <cmath>
#include <QFile>
#include <QDataStream>
#include <QIODevice>
#include <QDebug>

const float SparseField::DEFAULT_FIELD_VALUE = 0.0f;


Vec3i SparseFieldCoordinates::getOffsetTo(const SparseFieldCoordinates& other) const {
    Vec3i offset(0, 0, 0);
    const float eps = 1.e-4;
    if ((spacing - other.spacing).length() > eps) {
        throw std::runtime_error("Error in getOffsetTo. SparseFields have different spacing");
    }
    for (int i=0; i<=2; ++i) {
        double fractionPart, integerPart;
        fractionPart = modf((origin[i] - other.origin[i])/spacing[i], &integerPart);
        if (fabs(fractionPart) > eps) {
            throw std::runtime_error("Error in getOffsetTo. Origins are not shifted to integer multiples");
        }
        if (integerPart < 0) {
            offset[i] = int(integerPart - 0.5);
        }
        else {
            offset[i] = int(integerPart + 0.5);
        }
    }
    return offset;
}


SparseField multiply(const SparseField& fs1,
                     const SparseField& fs2)
{
    const Vec3i offset = fs1.getCoordinates().getOffsetTo(fs2.getCoordinates());
    SparseField result = SparseField(fs1.getCoordinates());

    int newValueIdx = 0;
    for (SparseField::LocationIndexToValueIndexMap::ConstIterator it1 = fs1.mIndexMap.begin(); it1 != fs1.mIndexMap.end(); ++it1) {
        const SparseField::LocationIndexT index1 = it1.key();
        const Vec3i loc1 = fs1.getXYZFromIndex(index1);
        const Vec3i loc2 = loc1 + offset;
        const SparseField::LocationIndexT index2 = fs2.getIndexFromXYZ(loc2);

        SparseField::LocationIndexToValueIndexMap::ConstIterator it2 = fs2.mIndexMap.find(index2);
        if (it2 != fs2.mIndexMap.end()) {
            const float& v1 = fs1.mField[it1.value()];
            const float& v2 = fs2.mField[it2.value()];
            result.mIndexMap.insert(index1, newValueIdx);
            result.mField.push_back(v1 * v2);
            ++newValueIdx;
        }
    }
    return result;
}


SparseField operator+(const SparseField& fs1,
                      const SparseField& fs2)
{
    SparseFieldCoordinates unionGrid(fs1.getCoordinates());
    unionGrid.extendBy(fs2.getCoordinates());
    SparseField result(unionGrid);

    const Vec3i offset1 = unionGrid.getOffsetTo(fs1.getCoordinates());
    const Vec3i offset2 = unionGrid.getOffsetTo(fs2.getCoordinates());

    for (SparseField::ConstIterator it1 = fs1.constBegin(); it1 != fs1.constEnd(); ++it1) {
        result.addValue(it1.location - offset1, it1.value);
    }
    for (SparseField::ConstIterator it2 = fs2.constBegin(); it2 != fs2.constEnd(); ++it2) {
        result.addValue(it2.location - offset2, it2.value);
    }

    return result;
}


SparseField divide(const SparseField& fs1,
                   const SparseField& fs2)
{
    const Vec3i offset = fs1.getCoordinates().getOffsetTo(fs2.getCoordinates());
    SparseField result = SparseField(fs1.getCoordinates());

    int newValueIdx = 0;
    // Only create value if fs1 and fs2 have value at same location (assumed to be non-zero)
    for (SparseField::LocationIndexToValueIndexMap::ConstIterator it1 = fs1.mIndexMap.constBegin(); it1 != fs1.mIndexMap.constEnd(); ++it1) {
        const SparseField::LocationIndexT index1 = it1.key();
        const Vec3i loc1 = fs1.getXYZFromIndex(index1);
        const Vec3i loc2 = loc1 + offset;
        const SparseField::LocationIndexT index2 = fs2.getIndexFromXYZ(loc2);

        SparseField::LocationIndexToValueIndexMap::ConstIterator it2 = fs2.mIndexMap.find(index2);
        if (it2 != fs2.mIndexMap.end()) {
            const float& v1 = fs1.mField[it1.value()];
            const float& v2 = fs2.mField[it2.value()];
            result.mIndexMap.insert(index1, newValueIdx);
            result.mField.push_back(v1 / v2);
            ++newValueIdx;
        }
    }
    return result;
}


SparseField SparseField::multiply(const float& factor) const
{
    SparseField result(*this);
    result.multiply(factor);
    return result;
}


SparseField::LocationIndexT SparseField::getIndexFromXYZ(const Vec3i& location) const {
    return location.getZ() * mDimensions.getX() * mDimensions.getY() + location.getY() * mDimensions.getX() + location.getX();
}


Vec3i SparseField::getXYZFromIndex(const SparseField::LocationIndexT& index) const {
    Vec3i xyz;
    SparseField::LocationIndexT remainder;

    xyz.setZ(index / (mDimensions.getX() * mDimensions.getY()));
    remainder = index - (xyz.getZ() * mDimensions.getX() * mDimensions.getY());
    xyz.setY(remainder / mDimensions.getX());
    remainder -= xyz.getY() * mDimensions.getX();
    xyz.setX(remainder);

    if (getIndexFromXYZ(xyz) != index) {
        throw std::runtime_error("Error in getXYZFromIndex");
    }
    return xyz;
}


SparseField::SparseField() :
    mDimensions(0, 0, 0),
    mOrigin(0, 0, 0),
    mVoxelSize(1.0f, 1.0f, 1.0f)
{
    mField.clear();
}


SparseField::SparseField(const Vec3i& dims,
                         const Vec3f& origin,
                         const Vec3f& voxelSize) :
    mDimensions(dims),
    mOrigin(origin),
    mVoxelSize(voxelSize)
{
    mField.clear();
}


SparseField::SparseField(const SparseFieldCoordinates &coords) :
    mDimensions(coords.dimensions),
    mOrigin(coords.origin),
    mVoxelSize(coords.spacing)
{
    mField.clear();
}


SparseField::SparseField(const Vec3i &dims,
                         const Locations &locations,
                         const SparseField::Field &field,
                         const Vec3f &origin,
                         const Vec3f &voxelSize) :
    mDimensions(dims),
    mOrigin(origin),
    mVoxelSize(voxelSize),
    mField(field)
{
    if (locations.size() != field.size()) {
        throw std::runtime_error("Cannot create SparseField: locations and field have different size");
    }

    if (locations.size() == 0) {
        return;
    }

    ValueIndexT valueIndex = 0;
    for (Locations::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        const Vec3i& loc = *it;
        const LocationIndexT locationIdx = getIndexFromXYZ(loc);
        mIndexMap[locationIdx] = valueIndex;
        ++valueIndex;
    }
}


Vec3i SparseField::getDimensions() const {
    return mDimensions;
}


Vec3f SparseField::getOrigin() const {
    return mOrigin;
}


Vec3f SparseField::getVoxelSize() const {
    return mVoxelSize;
}


void SparseField::setFieldValue(const Vec3i &location, const float value) {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    const SparseField::LocationIndexToValueIndexMap::const_iterator it = mIndexMap.find(locationIdx);
    if (it == mIndexMap.end()) {
        throw std::runtime_error("No value for position");
    }
    const int valueIdx = int(it.value());
    mField[valueIdx] = value;
}


int SparseField::save(const SparseField* fs, const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file" << fileName << "for writing.";
        return 0;
    }

    QDataStream out(&file);
    writeToStream(fs, out);

    return 1;
}


SparseField *SparseField::load(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << QString("Cannot open file for reading: %1").arg(fileName);
        return 0;
    }

    QDataStream in(&file);
    SparseField* fs = readFromStream(in);
    return fs;
}


void SparseField::writeToStream(const SparseField *fs, QDataStream &out) {
    const QString version = "2.0.0";
    out << version;
    out << fs->mDimensions.getX() << fs->mDimensions.getY() << fs->mDimensions.getZ();
    out << fs->mOrigin.getX() << fs->mOrigin.getY() << fs->mOrigin.getZ();
    out << fs->mVoxelSize.getX() << fs->mVoxelSize.getY() << fs->mVoxelSize.getZ();
    out << fs->mIndexMap;
    out << fs->mField;
}


SparseField* SparseField::readFromStream(QDataStream &in) {
    QString version;
    in >> version;

    if (version != "2.0.0") {
        throw std::runtime_error("Invalid file format version");
    }

    Vec3i dims;
    in >> dims[0];
    in >> dims[1];
    in >> dims[2];

    Vec3f origin;
    in >> origin[0];
    in >> origin[1];
    in >> origin[2];

    Vec3f voxelSize;
    in >> voxelSize[0];
    in >> voxelSize[1];
    in >> voxelSize[2];

    SparseField* fs = new SparseField(dims, origin, voxelSize);
    in >> fs->mIndexMap;
    in >> fs->mField;

    return fs;
}


SparseFieldCoordinates SparseField::getCoordinates() const
{
    SparseFieldCoordinates coords;
    coords.dimensions = mDimensions;
    coords.spacing = mVoxelSize;
    coords.origin = mOrigin;
    return coords;
}


SparseField::ConstIterator SparseField::constBegin() const
{
    return SparseField::ConstIterator(*this, ITERATOR_BEGIN);
}


SparseField::ConstIterator SparseField::constEnd() const
{
    return SparseField::ConstIterator(*this, ITERATOR_END);
}


float SparseField::getFieldValue(const Vec3i &location) const {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    const SparseField::LocationIndexToValueIndexMap::const_iterator it = mIndexMap.find(locationIdx);
    if (it == mIndexMap.end()) {
        return DEFAULT_FIELD_VALUE;
    }
    const int valueIdx = int(it.value());
    return mField[valueIdx];
}


void SparseField::getFieldValues(SparseField::Locations &locations, SparseField::Field &values) const
{
    locations.clear();
    values.clear();

    for (LocationIndexToValueIndexMap::ConstIterator it=mIndexMap.begin(); it!=mIndexMap.end(); ++it) {
        const SparseField::LocationIndexT& index = it.key();
        const int valueIndex = it.value();
        locations.append(getXYZFromIndex(index));
        values.append(mField.value(valueIndex));
    }
}


bool SparseField::hasFieldValue(const Vec3i &location) const {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    return mIndexMap.contains(locationIdx);
}


float SparseField::getFieldSum() const
{
    float sum = 0.0f;

    for (Field::ConstIterator it = mField.constBegin(); it != mField.constEnd(); ++it) {
        sum += *it;
    }

    return sum;
}


void SparseField::addValue(const Vec3i &location, const float value)
{
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    const SparseField::LocationIndexToValueIndexMap::iterator it = mIndexMap.find(locationIdx);
    if (it == mIndexMap.end()) {
        mIndexMap.insert(locationIdx, mField.size());
        mField.append(value);
    }
    else {
        const int valueIdx = int(it.value());
        mField[valueIdx] += value;
    }
}


void SparseField::multiply(const float factor)
{
    for (Field::iterator it=mField.begin(); it!=mField.end(); ++it) {
        *it *= factor;
    }
}


Vec3i SparseField::getVoxelContainingPoint(const Vec3f& p) const
{
    Vec3i result;
    Vec3f shiftedPos = p - mOrigin;

    for (int i=0; i<=2; ++i) {
        if (shiftedPos[i] < 0.0f) {
            throw std::runtime_error("Error: point not in grid (too small)");
        }
        result[i] = int(shiftedPos[i] / mVoxelSize[i]);
        if (result[i] >= mDimensions[i]) {
            throw std::runtime_error("Error: point not in grid (too large)");
        }
    }

    return result;
}


BoundingBox SparseField::getVoxelBox(const Vec3i &v) const
{
    Vec3f minBox, maxBox;

    minBox[0] = mOrigin[0] + v[0]*mVoxelSize[0];
    minBox[1] = mOrigin[1] + v[1]*mVoxelSize[1];
    minBox[2] = mOrigin[2] + v[2]*mVoxelSize[2];

    maxBox[0] = mOrigin[0] + (v[0]+1)*mVoxelSize[0];
    maxBox[1] = mOrigin[1] + (v[1]+1)*mVoxelSize[1];
    maxBox[2] = mOrigin[2] + (v[2]+1)*mVoxelSize[2];

    return BoundingBox(minBox, maxBox);
}


BoundingBox SparseField::getNonZeroBox() const
{
    Vec3i boxMin(1, 1, 1), boxMax(-1, -1, -1);
    for (SparseField::ConstIterator it=constBegin(); it!=constEnd(); ++it) {
        if (it == constBegin()) {
            if (it.value > 1.e-4) {
                boxMin = it.location;
                boxMax = it.location;
            }
        }
        else {
            if (it.value > 1.e-4) {
                if (it.location.getX() < boxMin.getX()) boxMin.setX(it.location.getX());
                if (it.location.getY() < boxMin.getY()) boxMin.setY(it.location.getY());
                if (it.location.getZ() < boxMin.getZ()) boxMin.setZ(it.location.getZ());
                if (it.location.getX() > boxMax.getX()) boxMax.setX(it.location.getX());
                if (it.location.getY() > boxMax.getY()) boxMax.setY(it.location.getY());
                if (it.location.getZ() > boxMax.getZ()) boxMax.setZ(it.location.getZ());
            }
        }
    }

    if (boxMin == Vec3i(1, 1, 1) && boxMax == Vec3i(-1, -1, -1)) {
        return BoundingBox();
    }

    Vec3f boxMinCoords, boxMaxCoords;
    for (int i=0; i<=2; ++i) {
        boxMinCoords[i] = mOrigin[i] + boxMin[i] * mVoxelSize[i];
        boxMaxCoords[i] = mOrigin[i] + (boxMax[i]+1) * mVoxelSize[i];
    }

    return BoundingBox(boxMinCoords, boxMaxCoords);
}



void SparseFieldCoordinates::extendBy(const SparseFieldCoordinates &other)
{
    getOffsetTo(other); // Ensure that grid cells are aligned
    Vec3f newOrigin;
    Vec3i newDimensions;
    for (int i=0; i<=2; ++i) {
        newOrigin[i] = qMin(origin[i], other.origin[i]);
        const float oldMax      = origin[i] + dimensions[i] * spacing[i];
        const float otherOldMax = other.origin[i] + other.dimensions[i] * other.spacing[i];
        newDimensions[i] = int((qMax(oldMax, otherOldMax)-newOrigin[i])/spacing[i] + 0.5);
    }
    origin = newOrigin;
    dimensions = newDimensions;
}


void SparseFieldCoordinates::print() const
{
    qDebug() << "Dimensions:" << dimensions.getX() << dimensions.getY() << dimensions.getZ();
    qDebug() << "Spacing:   " << spacing.getX() << spacing.getY() << spacing.getZ();
    qDebug() << "Origin:    " << origin.getX() << origin.getY() << origin.getZ();
}