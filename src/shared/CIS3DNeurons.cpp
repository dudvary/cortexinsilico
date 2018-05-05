#include "CIS3DNeurons.h"
#include <stdexcept>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QTextStream>
#include <QBitArray>


Neurons::Neurons()
{
}


Neurons::Neurons(const QString &csvFile)
{
    loadCSV(csvFile);
}


void Neurons::addNeuron(const NeuronProperties &neuronProps)
{
    const int id = neuronProps.id;
    if (mPropsMap.contains(id)) {
        throw std::runtime_error("Error in addNeuron: Neuron ID exists");
    }
    mPropsMap.insert(id, neuronProps);
}


bool Neurons::exists(const int id) const
{
    return mPropsMap.contains(id);
}


int Neurons::getCellTypeId(const int id) const {
    if (mPropsMap.contains(id)) {
        return mPropsMap.value(id).cellTypeId;
    }
    QString msg = QString("Error in getCellTypeId: Neuron ID %1 does not exist.").arg(id);
    throw std::runtime_error(qPrintable(msg));
}


int Neurons::getRegionId(const int id) const {
    const PropsMap::const_iterator it = mPropsMap.constFind(id);
    if (it == mPropsMap.constEnd()) {
        const QString msg = QString("Error in getRegion: Neuron ID %1 does not exist.").arg(id);
        throw std::runtime_error(qPrintable(msg));
    }
    return it->regionId;
}


Vec3f Neurons::getSomaPosition(const int id) const {
    if (mPropsMap.contains(id)) {
        const NeuronProperties& props = mPropsMap.value(id);
        return Vec3f(props.somaX, props.somaY, props.somaZ);
    }
    QString msg = QString("Error in getCellTypeId: Neuron ID %1 does not exist.").arg(id);
    throw std::runtime_error(qPrintable(msg));
}


CIS3D::SynapticSide Neurons::getSynapticSide(const int id) const
{
    const PropsMap::const_iterator it = mPropsMap.constFind(id);
    if (it == mPropsMap.constEnd()) {
        const QString msg = QString("Error in getSynapticSide: Neuron ID %1 does not exist.").arg(id);
        throw std::runtime_error(qPrintable(msg));
    }
    return it->synapticSide;
}


QList<int> Neurons::getFilteredNeuronIds(const QList<int> &includedCellTypeIds,
                                         const QList<int> &includedRegionIds,
                                         const CIS3D::SynapticSide synapticSide) const
{
    const bool allCellTypesIncluded = (includedCellTypeIds.size() == 0);
    const bool allRegionsIncluded = (includedRegionIds.size() == 0);

    QBitArray selectedCTs;
    if (!allCellTypesIncluded) {
        int maxCT = -1;
        for (int i=0; i<includedCellTypeIds.size(); ++i) {
            if (includedCellTypeIds[i] > maxCT) {
                maxCT = includedCellTypeIds[i];
            }
        }
        selectedCTs.resize(maxCT+1);
        selectedCTs.fill(false);
        for (int i=0; i<includedCellTypeIds.size(); ++i) {
            selectedCTs.setBit(includedCellTypeIds[i], true);
        }
    }

    QBitArray selectedRegions;
    if (!allRegionsIncluded) {
        int maxReg = -1;
        for (int i=0; i<includedRegionIds.size(); ++i) {
            if (includedRegionIds[i] > maxReg) {
                maxReg = includedRegionIds[i];
            }
        }
        selectedRegions.resize(maxReg+1);
        selectedRegions.fill(false);
        for (int i=0; i<includedRegionIds.size(); ++i) {
            selectedRegions.setBit(includedRegionIds[i], true);
        }
    }

    QList<int> result;

    for (QMap<int, NeuronProperties>::ConstIterator it = mPropsMap.constBegin(); it != mPropsMap.constEnd(); ++it) {
        const int neuronId = it.key();
        const NeuronProperties& props = it.value();
        if ((allCellTypesIncluded || (props.cellTypeId < selectedCTs.size()     && selectedCTs.at(props.cellTypeId))) &&
            (allRegionsIncluded   || (props.regionId   < selectedRegions.size() && selectedRegions.at(props.regionId))) &&
            ((synapticSide == CIS3D::BOTH_SIDES) || (getSynapticSide(neuronId) == CIS3D::BOTH_SIDES) || (synapticSide == getSynapticSide(neuronId))))
        {
            result.append(neuronId);
        }
    }

    return result;
}


QList<int> Neurons::getFilteredNeuronIds(const SelectionFilter& filter) const {
    const bool allCellTypesIncluded = (filter.cellTypeIds.size() == 0);
    const bool allRegionsIncluded = (filter.regionIds.size() == 0);
    const bool allNearestColumnsIncluded = (filter.nearestColumnIds.size() == 0);
    const bool allLaminarLocationsIncluded = (filter.laminarLocations.size() == 0);

    QBitArray selectedCTs;
    if (!allCellTypesIncluded) {
        int maxCT = -1;
        for (int i=0; i<filter.cellTypeIds.size(); ++i) {
            if (filter.cellTypeIds[i] > maxCT) {
                maxCT = filter.cellTypeIds[i];
            }
        }
        selectedCTs.resize(maxCT+1);
        selectedCTs.fill(false);
        for (int i=0; i<filter.cellTypeIds.size(); ++i) {
            selectedCTs.setBit(filter.cellTypeIds[i], true);
        }
    }

    QBitArray selectedRegions;
    if (!allRegionsIncluded) {
        int maxReg = -1;
        for (int i=0; i<filter.regionIds.size(); ++i) {
            if (filter.regionIds[i] > maxReg) {
                maxReg = filter.regionIds[i];
            }
        }
        selectedRegions.resize(maxReg+1);
        selectedRegions.fill(false);
        for (int i=0; i<filter.regionIds.size(); ++i) {
            selectedRegions.setBit(filter.regionIds[i], true);
        }
    }

    QBitArray selectedNearestColumns;
    if (!allNearestColumnsIncluded) {
        int maxNC = -1;
        for (int i=0; i<filter.nearestColumnIds.size(); ++i) {
            if (filter.nearestColumnIds[i] > maxNC) {
                maxNC = filter.nearestColumnIds[i];
            }
        }
        selectedNearestColumns.resize(maxNC+1);
        selectedNearestColumns.fill(false);
        for (int i=0; i<filter.nearestColumnIds.size(); ++i) {
            selectedNearestColumns.setBit(filter.nearestColumnIds[i], true);
        }
    }

    QBitArray selectedLaminarLocations;
    if (!allLaminarLocationsIncluded) {
        selectedLaminarLocations.resize(4);
        selectedLaminarLocations.fill(false);
        for (int i=0; i<filter.laminarLocations.size(); ++i) {
            selectedLaminarLocations.setBit(int(filter.laminarLocations[i]), true);
        }
    }

    QList<int> result;

    for (QMap<int, NeuronProperties>::ConstIterator it = mPropsMap.constBegin(); it != mPropsMap.constEnd(); ++it) {
        const int neuronId = it.key();
        const NeuronProperties& props = it.value();
        if ((allCellTypesIncluded || (props.cellTypeId < selectedCTs.size()     && selectedCTs.at(props.cellTypeId))) &&
            (allRegionsIncluded   || (props.regionId   < selectedRegions.size() && selectedRegions.at(props.regionId))) &&
            (allNearestColumnsIncluded  || (props.nearestColumnId  < selectedNearestColumns.size() && selectedNearestColumns.at(props.nearestColumnId))) &&
            (allLaminarLocationsIncluded  || selectedLaminarLocations.at(int(props.loc))) &&
            ((filter.synapticSide == CIS3D::BOTH_SIDES) || (getSynapticSide(neuronId) == CIS3D::BOTH_SIDES) || (filter.synapticSide == getSynapticSide(neuronId))))
        {
            result.append(neuronId);
        }
    }

    return result;

}


void Neurons::saveCSV(const QString &fileName) const
{
    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const QString msg = QString("Error saving neurons file. Could not open file %1").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    QTextStream out(&file);
    const QChar sep = ',';

    out << "ID" << sep;
    out << "SomaX" << sep;
    out << "SomaY" << sep;
    out << "SomaZ" << sep;
    out << "CellTypeID" << sep;
    out << "NearestColumnID" << sep;
    out << "RegionID" << sep;
    out << "LaminarLocation" << sep;
    out << "SynapticSide" << "\n";

    for (PropsMap::ConstIterator it = mPropsMap.begin(); it != mPropsMap.end(); ++it) {
        const NeuronProperties& props = it.value();
        out << props.id << sep;
        out << props.somaX << sep;
        out << props.somaY << sep;
        out << props.somaZ << sep;
        out << props.cellTypeId << sep;
        out << props.nearestColumnId << sep;
        out << props.regionId << sep;
        out << int(props.loc) << sep;
        out << int(props.synapticSide) << "\n";
    }
}


void Neurons::loadCSV(const QString &fileName)
{
    QFile file(fileName);
    QTextStream(stdout) << "[*] Reading neurons from " << fileName << "\n";

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString msg = QString("Error reading neurons file. Could not open file %1").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    const QChar sep = ',';
    QTextStream in(&file);

    int lineCount = 1;
    QString line = in.readLine();
    if (line.isNull()) {
        const QString msg = QString("Error reading neurons file %1. No content.").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    QStringList parts = line.split(sep);
    if (parts.size() != 9 ||
        parts[0] != "ID" ||
        parts[1] != "SomaX" ||
        parts[2] != "SomaY" ||
        parts[3] != "SomaZ" ||
        parts[4] != "CellTypeID" ||
        parts[5] != "NearestColumnID" ||
        parts[6] != "RegionID" ||
        parts[7] != "LaminarLocation" ||
        parts[8] != "SynapticSide" )
    {
        const QString msg = QString("Error reading neurons file %1. Invalid header columns.").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    line = in.readLine();
    lineCount += 1;

    while (!line.isNull()) {
        parts = line.split(sep);
        if (parts.size() != 9) {
            const QString msg = QString("Error reading neurons file %1. Invalid columns.").arg(fileName);
            throw std::runtime_error(qPrintable(msg));
        }

        NeuronProperties props;
        props.id = parts[0].toInt();
        props.somaX = parts[1].toFloat();
        props.somaY = parts[2].toFloat();
        props.somaZ = parts[3].toFloat();
        props.cellTypeId = parts[4].toInt();
        props.nearestColumnId = parts[5].toInt();
        props.regionId = parts[6].toInt();
        props.loc = static_cast<CIS3D::LaminarLocation>(parts[7].toInt());
        props.synapticSide = static_cast<CIS3D::SynapticSide>(parts[8].toInt());

        addNeuron(props);

        line = in.readLine();
        lineCount += 1;
    }
    QTextStream(stdout) << "[*] Completed reading " <<  mPropsMap.size() << " neurons.\n";
}

