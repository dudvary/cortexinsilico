#include "CIS3DSparseField.h"
#include <stdio.h>
#include <QDataStream>
#include <QSharedMemory>
#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QVector>
#include <cmath>
#include <stdexcept>
#include <QBuffer>


const float SparseField::DEFAULT_FIELD_VALUE = 0.0f;

/**
    Determines shift in origin towards other SparseFieldCoordinates.
    @param other The other SparseFieldCoordinates.
    @return The shift in orgin.
    @throws runtime_error if the spacing differs or if the shift is not an
        integer multiple of the voxel spacing.
*/
Vec3i SparseFieldCoordinates::getOffsetTo(const SparseFieldCoordinates& other) const {
    Vec3i offset(0, 0, 0);
    const float eps = 1.e-4;
    if ((spacing - other.spacing).length() > eps) {
        throw std::runtime_error("Error in getOffsetTo. SparseFields have different spacing");
    }
    for (int i = 0; i <= 2; ++i) {
        double fractionPart, integerPart;
        fractionPart = modf((origin[i] - other.origin[i]) / spacing[i], &integerPart);
        if (fabs(fractionPart) > eps) {
            throw std::runtime_error(
                "Error in getOffsetTo. Origins are not shifted to integer multiples");
        }
        if (integerPart < 0) {
            offset[i] = int(integerPart - 0.5);
        } else {
            offset[i] = int(integerPart + 0.5);
        }
    }
    return offset;
}

/**
    Multiplies the grid values of the first SparseField with the grid values of
    the second SparseField and returns the result in a new SparseField. The
    multiplication is only carried out if the respective location is also defined
    in the second SparseField.
    @param fs1 The first sparse field.
    @param fs2 The second sparse field.
    @return The resulting sparse field.
    @throws runtime_error if the spacing differs or if the shift between
        both fields is not an integer multiple of the voxel spacing.
*/
SparseField multiply(const SparseField& fs1, const SparseField& fs2) {
    const Vec3i offset = fs1.getCoordinates().getOffsetTo(fs2.getCoordinates());
    SparseField result = SparseField(fs1.getCoordinates());

    int newValueIdx = 0;
    for (SparseField::LocationIndexToValueIndexMap::ConstIterator it1 = fs1.mIndexMap.begin();
         it1 != fs1.mIndexMap.end(); ++it1) {
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

/**
    Applies an generic operator function on all values of the sparse field.
    @param fieldOperator The operator function.
*/
void SparseField::applyOperator(SparseFieldOperator& fieldOperator) {
    for (int i = 0; i < mField.size(); i++) {
        mField[i] = fieldOperator.calculate(mField[i]);
    }
}

/**
  Creates a union grid of the specified SparseFields. If a location is defined
  in both SparseFields, the resulting location is assigned the sum from both
  SparseFields.
  @param fs1 The first sparse field.
  @param fs2 The second sparse field.
  @return The resulting sparse field.
  @throws runtime_error if the spacing differs or if the shift between
      both fields is not an integer multiple of the voxel spacing.
*/
SparseField operator+(const SparseField& fs1, const SparseField& fs2) {
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

/**
    Divides the grid values of the first SparseField by the grid values of
    the second SparseField and returns the result in a new SparseField. The
    division is only carried out if the respective location is also defined
    in the second SparseField.
    @param fs1 The first sparse field.
    @param fs2 The second sparse field.
    @return The resulting sparse field.
    @throws runtime_error if the spacing differs or if the shift between
        both fields is not an integer multiple of the voxel spacing.
*/
SparseField divide(const SparseField& fs1, const SparseField& fs2) {
    const Vec3i offset = fs1.getCoordinates().getOffsetTo(fs2.getCoordinates());
    SparseField result = SparseField(fs1.getCoordinates());

    int newValueIdx = 0;
    // Only create value if fs1 and fs2 have value at same location (assumed to be non-zero)
    for (SparseField::LocationIndexToValueIndexMap::ConstIterator it1 = fs1.mIndexMap.constBegin();
         it1 != fs1.mIndexMap.constEnd(); ++it1) {
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

/**
    Creates a new grid by multiplying all values in the current
    grid by the specified factor.
    @param factor The multiplication factor.
*/
SparseField SparseField::multiply(const float& factor) const {
    SparseField result(*this);
    result.multiply(factor);
    return result;
}

SparseField::LocationIndexT SparseField::getIndexFromXYZ(const Vec3i& location) const {
    return location.getZ() * mDimensions.getX() * mDimensions.getY() +
           location.getY() * mDimensions.getX() + location.getX();
}

Vec3i SparseField::getXYZFromIndex(const SparseField::LocationIndexT& index) const {
    Vec3i xyz;
    SparseField::LocationIndexT remainder;

    if (index < 0) {
        throw std::runtime_error("Error in getXYZFromIndex: index < 0");
    }

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

/**
    Constructor.
    Initializes origin and dimenions to (0,0,0) and voxel size to (1,1,1).
*/
SparseField::SparseField() : mDimensions(0, 0, 0), mOrigin(0, 0, 0), mVoxelSize(1.0f, 1.0f, 1.0f) {
    mField.clear();
}

/**
    Constructor.
    @param dims The dimensions of the grid.
    @param origin The origin of the grid.
    @param voxelSize The spacing of the grid.
*/
SparseField::SparseField(const Vec3i& dims, const Vec3f& origin, const Vec3f& voxelSize)
    : mDimensions(dims), mOrigin(origin), mVoxelSize(voxelSize) {
    mField.clear();
}

/**
    Constructor.
    @param coords The SparseFieldCoordinates specifying origin, dimensions, and spacing.
*/
SparseField::SparseField(const SparseFieldCoordinates& coords)
    : mDimensions(coords.dimensions), mOrigin(coords.origin), mVoxelSize(coords.spacing) {
    mField.clear();
}

/**
    Constructor.
    @param dims The dimensions of the grid.
    @param locations The locations.
    @param field The values at the locations.
    @param origin The origin of the grid.
    @param voxelSize The spacing of the grid.
    @throws runtime_error if the number of entries in locations and field differs.
*/
SparseField::SparseField(const Vec3i& dims, const Locations& locations,
                         const SparseField::Field& field, const Vec3f& origin,
                         const Vec3f& voxelSize)
    : mDimensions(dims), mOrigin(origin), mVoxelSize(voxelSize), mField(field) {
    if (locations.size() != field.size()) {
        throw std::runtime_error(
            "Cannot create SparseField: locations and field have different size");
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

std::map<int,float> SparseField::getModifiedCopy(float coefficient, float eps){
    std::map<int, float> field;
    for(LocationIndexToValueIndexMap::ConstIterator it = mIndexMap.begin(); it != mIndexMap.end(); ++it){           
        float value = mField[it.value()];
        if(value > eps){
            value = coefficient * log(value);
            std::pair<int, float> pair(it.key(),value);     
            field.insert(pair);
        }
        
    }
    return field; 
}

Vec3f SparseField::getSpatialLocation(int voxelIndex){
    Vec3i gridLocation = getXYZFromIndex(voxelIndex);
    Vec3f spatialLocation;
    for(int i=0; i<3; i++){
        spatialLocation[i] = mOrigin[i] + (gridLocation[i] + 0.5) * mVoxelSize[i];
    }   
    return spatialLocation;
}

/**
    Returns the dimensions of the grid.
    @return The dimensions.
*/
Vec3i SparseField::getDimensions() const { return mDimensions; }

/**
    Returns the origin of the grid.
    @return The origin.
*/
Vec3f SparseField::getOrigin() const { return mOrigin; }

/**
    Returns the spacing of the grid cells.
    @return The voxel size.
*/
Vec3f SparseField::getVoxelSize() const { return mVoxelSize; }

/**
    Sets the value at the specified location.
    @param location The location.
    @param value The value.
    @throws runtime_error if the location has currently no value.
*/
void SparseField::setFieldValue(const Vec3i& location, const float value) {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    const SparseField::LocationIndexToValueIndexMap::const_iterator it =
        mIndexMap.find(locationIdx);
    if (it == mIndexMap.end()) {
        throw std::runtime_error("No value for position");
    }
    const int valueIdx = int(it.value());
    mField[valueIdx] = value;
}

/**
    Saves a SparseField to file.
    @param fs The SparseField to save.
    @param fileName The name of the file.
    @return 1 if successful, 0 otherwise.
*/
int SparseField::save(const SparseField* fs, const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file" << fileName << "for writing.";
        return 0;
    }

    QDataStream out(&file);
    writeToStream(fs, out);

    return 1;
}

/**
    Saves the SparseField as csv file of voxel locations (or
    spatial coordinates) associated with nonzero values.

    @param fileName The name of the file.
    @param spatial Use coordinates instead of voxel indices.
    @throws runtime_error if saving the file failed.
*/
void SparseField::saveCSV(const QString& fileName, const bool spatial) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QString msg = QString("Could not open file %1 for writing").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    const QChar sep(',');
    QTextStream out(&file);
    out << "locationX" << sep << "locationY" << sep << "locationZ" << sep << "value"
        << "\n";

    QVector<int> dimensions;
    dimensions.append(mDimensions[0]);
    dimensions.append(mDimensions[1]);
    dimensions.append(mDimensions[2]);
    for (SparseField::LocationIndexToValueIndexMap::ConstIterator it = mIndexMap.begin();
         it != mIndexMap.end(); ++it) {
        Vec3i location = getXYZFromIndex(it.key());
        float x, y, z;
        if (spatial) {
            x = mOrigin[0] + location[0] * mVoxelSize[0] + 0.5 * mVoxelSize[0];
            y = mOrigin[1] + location[1] * mVoxelSize[1] + 0.5 * mVoxelSize[1];
            z = mOrigin[2] + location[2] * mVoxelSize[2] + 0.5 * mVoxelSize[2];
        } else {
            x = (float)location[0] + 1;
            y = (float)location[1] + 1;
            z = (float)location[2] + 1;
        }
        const float value = mField[it.value()];

        if (value > 0) {
            out << x << sep << y << sep << z << sep << value << "\n";
        }
    }
}

/**
    Loads a SparseField from file.
    @param fileName The name of the file.
    @return The SparseField if successful, 0 otherwise.
    @throws runtime_error in case of a version mismatch.
*/
SparseField* SparseField::load(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << QString("Cannot open file for reading: %1").arg(fileName);
        return 0;
    }

    // qDebug() << "Loading SparseField from file:" << fileName;
    QDataStream in(&file);
    SparseField* fs = readFromStream(in);
    return fs;
}

/**
    Writes a SparseField to the specified data stream.
    @param fs The SparseField to write.
    @param out The data stream.
*/
void SparseField::writeToStream(const SparseField* fs, QDataStream& out) {
    const QString version = "2.0.0";
    out << version;
    out << fs->mDimensions.getX() << fs->mDimensions.getY() << fs->mDimensions.getZ();
    out << fs->mOrigin.getX() << fs->mOrigin.getY() << fs->mOrigin.getZ();
    out << fs->mVoxelSize.getX() << fs->mVoxelSize.getY() << fs->mVoxelSize.getZ();
    out << fs->mIndexMap;
    out << fs->mField;
}

/**
    Reads a SparseField from the specified data stream.
    @param in The data stream.
    @return The SparseField.
    @throws runtime_error in case of a version mismatch.
*/
SparseField* SparseField::readFromStream(QDataStream& in) {
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

    // Sanity check for loaded data
    for (LocationIndexToValueIndexMap::const_iterator it = fs->mIndexMap.constBegin();
         it != fs->mIndexMap.constEnd(); ++it) {
        if (it.key() < 0) {
            throw std::runtime_error("Error in SparseField: negative location index");
        }
    }

    return fs;
}

/*
    Writes sparse field to memory under the specified key.
    @param key The memory segment.
    @field The sparse field.
*/
void SparseField::writeToMemory(QString key, SparseField* field){
    QSharedMemory sharedMemory(key);
    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);
    QDataStream out(&buffer);
    writeToStream(field, out);
    int size = buffer.size();
    if (!sharedMemory.create(size)) {
        qDebug() << "Unable to create shared memory segment" << key;
        return;
    }
    sharedMemory.lock();
    char *to = (char*)sharedMemory.data();
    const char *from = buffer.data().data();
    memcpy(to, from, qMin(sharedMemory.size(), size));
    sharedMemory.unlock();
}

/*
    Loads sparse field from memory under the specified key.
    @param key The memory segment.
    @return The sparse field.
*/
SparseField* SparseField::loadFromMemory(QString key){
    QSharedMemory sharedMemory(key);
    if (!sharedMemory.attach()) {
        qDebug() << "Unable to attach to shared memory segment" << key;                             
        return NULL;
    }

    QBuffer buffer;
    QDataStream in(&buffer);
    SparseField* field;
    sharedMemory.lock();
    buffer.setData((char*)sharedMemory.constData(), sharedMemory.size());
    buffer.open(QBuffer::ReadOnly);
    field = readFromStream(in);
    sharedMemory.unlock();
    sharedMemory.detach();

    return field;
}

/**
    Returns the coordinates of the SparseField (origin, dimension, voxel spacing).
    @return The SparseFieldCoordinates.
*/
SparseFieldCoordinates SparseField::getCoordinates() const {
    SparseFieldCoordinates coords;
    coords.dimensions = mDimensions;
    coords.spacing = mVoxelSize;
    coords.origin = mOrigin;
    return coords;
}

/*
    Determines unique ID for the specified voxel.

    @param locationLocal Position of the voxel relative to the origin.
    @param dimensions Number of voxels in each direction.
    @return The ID of the voxel.
*/
int SparseField::getVoxelId(Vec3i locationLocal, QVector<int> dimensions) {
    int nx = dimensions[0];
    int ny = dimensions[1];
    int ix = locationLocal[0];
    int iy = locationLocal[1];
    int iz = locationLocal[2];
    return iz * ny * nx + iy * nx + ix + 1;
}

SparseField::ConstIterator SparseField::constBegin() const {
    return SparseField::ConstIterator(*this, ITERATOR_BEGIN);
}

SparseField::ConstIterator SparseField::constEnd() const {
    return SparseField::ConstIterator(*this, ITERATOR_END);
}

/**
    Returns value at the specified location.
    @location The location.
    @return The value at the location if the location is defined
        DEFAULT_FIELD_VALUE otherwise.
*/
float SparseField::getFieldValue(const Vec3i& location) const {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    const SparseField::LocationIndexToValueIndexMap::const_iterator it =
        mIndexMap.find(locationIdx);
    if (it == mIndexMap.end()) {
        return DEFAULT_FIELD_VALUE;
    }
    const int valueIdx = int(it.value());
    return mField[valueIdx];
}

/**
    Copies the locations and the corresponding values contained in the
    grid.
    @param locations Is filled with the locations.
    @param values Is filled with the corresponding values.
*/
void SparseField::getFieldValues(SparseField::Locations& locations,
                                 SparseField::Field& values) const {
    locations.clear();
    values.clear();

    for (LocationIndexToValueIndexMap::ConstIterator it = mIndexMap.begin(); it != mIndexMap.end();
         ++it) {
        const SparseField::LocationIndexT& index = it.key();
        const int valueIndex = it.value();
        locations.append(getXYZFromIndex(index));
        values.append(mField.value(valueIndex));
    }
}

/**
    Determines whether the specified location has a value.
    @param location The location to check.
    @return True if a value is defined at the location.
*/
bool SparseField::hasFieldValue(const Vec3i& location) const {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    return mIndexMap.contains(locationIdx);
}

/**
    Returns the sum of all values in the grid.
    @return The sum.
*/
float SparseField::getFieldSum() const {
    float sum = 0.0f;

    for (Field::ConstIterator it = mField.constBegin(); it != mField.constEnd(); ++it) {
        sum += *it;
    }

    return sum;
}

/**
    Adds a value to the grid. If the location is already defined, the
    value is added to the current value in this cell.
    @param location The location.
    @param value The value to add.
*/
void SparseField::addValue(const Vec3i& location, const float value) {
    const LocationIndexT locationIdx = getIndexFromXYZ(location);
    const SparseField::LocationIndexToValueIndexMap::iterator it = mIndexMap.find(locationIdx);
    if (it == mIndexMap.end()) {
        mIndexMap.insert(locationIdx, mField.size());
        mField.append(value);
    } else {
        const int valueIdx = int(it.value());
        mField[valueIdx] += value;
    }
}

/**
    Multiplies all values in the grid by the specified factor.
    @param factor The multiplication factor.
*/
void SparseField::multiply(const float factor) {
    for (Field::iterator it = mField.begin(); it != mField.end(); ++it) {
        *it *= factor;
    }
}

/**
    Returns the grid location for the specified point.
    @param p The point in real world coordinates.
    @return The location in the grid.
    @throws runtime_error if the point is not covered by the grid.
*/
Vec3i SparseField::getVoxelContainingPoint(const Vec3f& p) const {
    Vec3i result;
    Vec3f shiftedPos = p - mOrigin;

    for (int i = 0; i <= 2; ++i) {
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

/*
    Checks whether the specfied position is within spatial bounds 
    of the field.
    @param p The position in spatial coordinates.
    @return True, if the position is within range.
*/
bool SparseField::inRange(const Vec3f& p) const {
    Vec3i result;
    Vec3f shiftedPos = p - mOrigin;

    for (int i = 0; i <= 2; ++i) {
        if (shiftedPos[i] < 0.0f) {
            return false;
        }
        result[i] = int(shiftedPos[i] / mVoxelSize[i]);
        if (result[i] >= mDimensions[i]) {
            return false;
        }
    }

    return true;
}

/**
    Determines bounding box covering the voxel specified by the grid
    location.
    @param v The grid location.
    @return The bounding box.
*/
BoundingBox SparseField::getVoxelBox(const Vec3i& v) const {
    Vec3f minBox, maxBox;

    minBox[0] = mOrigin[0] + v[0] * mVoxelSize[0];
    minBox[1] = mOrigin[1] + v[1] * mVoxelSize[1];
    minBox[2] = mOrigin[2] + v[2] * mVoxelSize[2];

    maxBox[0] = mOrigin[0] + (v[0] + 1) * mVoxelSize[0];
    maxBox[1] = mOrigin[1] + (v[1] + 1) * mVoxelSize[1];
    maxBox[2] = mOrigin[2] + (v[2] + 1) * mVoxelSize[2];

    return BoundingBox(minBox, maxBox);
}

/**
    Determines the bounding box covering all defined nonzero grid locations.
    @return The bounding box.
*/
BoundingBox SparseField::getNonZeroBox() const {
    Vec3i boxMin(1, 1, 1), boxMax(-1, -1, -1);
    for (SparseField::ConstIterator it = constBegin(); it != constEnd(); ++it) {
        if (it == constBegin()) {
            if (it.value > 1.e-4) {
                boxMin = it.location;
                boxMax = it.location;
            }
        } else {
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
    for (int i = 0; i <= 2; ++i) {
        boxMinCoords[i] = mOrigin[i] + boxMin[i] * mVoxelSize[i];
        boxMaxCoords[i] = mOrigin[i] + (boxMax[i] + 1) * mVoxelSize[i];
    }

    return BoundingBox(boxMinCoords, boxMaxCoords);
}

/**
    Extends this set of SparseFieldCoordinates, such that the other
    SparseFieldCoordinates are contained by shifting the origin and
    increasing the dimensions.
    @throws runtime_error if the spacing differs or if the shift is not an
        integer multiple of the voxel spacing.
*/
void SparseFieldCoordinates::extendBy(const SparseFieldCoordinates& other) {
    getOffsetTo(other);  // Ensure that grid cells are aligned
    Vec3f newOrigin;
    Vec3i newDimensions;
    for (int i = 0; i <= 2; ++i) {
        newOrigin[i] = qMin(origin[i], other.origin[i]);
        const float oldMax = origin[i] + dimensions[i] * spacing[i];
        const float otherOldMax = other.origin[i] + other.dimensions[i] * other.spacing[i];
        newDimensions[i] = int((qMax(oldMax, otherOldMax) - newOrigin[i]) / spacing[i] + 0.5);
    }
    origin = newOrigin;
    dimensions = newDimensions;
}

void SparseFieldCoordinates::print() const {
    qDebug() << "Dimensions:" << dimensions.getX() << dimensions.getY() << dimensions.getZ();
    qDebug() << "Spacing:   " << spacing.getX() << spacing.getY() << spacing.getZ();
    qDebug() << "Origin:    " << origin.getX() << origin.getY() << origin.getZ();
}
