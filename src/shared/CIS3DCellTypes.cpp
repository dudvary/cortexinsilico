#include "CIS3DCellTypes.h"
#include <stdexcept>
#include <QFile>
#include <QChar>
#include <QTextStream>
#include <QIODevice>
#include <QStringList>
#include <QDebug>

const QString FIELD_ID = "ID";
const QString FIELD_NAME = "Name";
const QString FIELD_ISEXCITATORY = "IsExcitatory";
const QString FIELD_COLOR_DEND_R = "ColorDendriteR";
const QString FIELD_COLOR_DEND_G = "ColorDendriteG";
const QString FIELD_COLOR_DEND_B = "ColorDendriteB";
const QString FIELD_COLOR_AXON_R = "ColorAxonR";
const QString FIELD_COLOR_AXON_G = "ColorAxonG";
const QString FIELD_COLOR_AXON_B = "ColorAxonB";


void CellTypes::addCellType(const int id, const QString &name, const bool isExcitatory) {
    if (mPropsMap.contains(id)) {
        throw std::runtime_error("Id for cell type already exists");
    }
    CellTypes::Properties props;
    props.name = name;
    props.isExcitatory = isExcitatory;
    mPropsMap[id] = props;
}


void CellTypes::setDendriteColor(const int id, const float r, const float g, const float b) {
    if (!mPropsMap.contains(id)) {
        throw std::runtime_error("Cell type id does not exist");
    }
    CellTypes::Properties& props = mPropsMap[id];
    props.colorDendR = r;
    props.colorDendG = g;
    props.colorDendB = b;
}


void CellTypes::setAxonColor(const int id, const float r, const float g, const float b) {
    if (!mPropsMap.contains(id)) {
        throw std::runtime_error("Cell type id does not exist");
    }
    CellTypes::Properties& props = mPropsMap[id];
    props.colorAxonR = r;
    props.colorAxonG = g;
    props.colorAxonB = b;
}


bool CellTypes::exists(const int id) const {
    return mPropsMap.contains(id);
}


bool CellTypes::isExcitatory(const int id) const {
    if (!mPropsMap.contains(id)) {
        throw std::runtime_error("Cell type id does not exist");
    }
    return mPropsMap.value(id).isExcitatory;
}


QString CellTypes::getName(const int id) const
{
    QMap<int, Properties>::ConstIterator it = mPropsMap.find(id);
    if (it == mPropsMap.constEnd()) {
        const QString msg = QString("Cell type id %1 does not exist").arg(id);
        throw std::runtime_error(qPrintable(msg));
    }
    return it.value().name;
}


Vec3f CellTypes::getDendriteColor(const int id) const {
    QMap<int, Properties>::ConstIterator it = mPropsMap.find(id);
    if (it == mPropsMap.constEnd()) {
        const QString msg = QString("Cell type id %1 does not exist").arg(id);
        throw std::runtime_error(qPrintable(msg));
    }

    Vec3f col;
    col.setX(it.value().colorDendR);
    col.setY(it.value().colorDendG);
    col.setZ(it.value().colorDendB);
    return col;
}


Vec3f CellTypes::getAxonColor(const int id) const {
    QMap<int, Properties>::ConstIterator it = mPropsMap.find(id);
    if (it == mPropsMap.constEnd()) {
        const QString msg = QString("Cell type id %1 does not exist").arg(id);
        throw std::runtime_error(qPrintable(msg));
    }

    Vec3f col;
    col.setX(it.value().colorAxonR);
    col.setY(it.value().colorAxonG);
    col.setZ(it.value().colorAxonB);
    return col;
}


int CellTypes::getId(const QString& name) const {
    for (QMap<int, Properties>::ConstIterator it = mPropsMap.constBegin(); it != mPropsMap.constEnd(); ++it) {
        if (it.value().name == name) {
            return it.key();
        }
    }
    const QString msg = QString("Cell type name %1 does not exist").arg(name);
    throw std::runtime_error(qPrintable(msg));
}


QList<int> CellTypes::getAllCellTypeIds() const {
    return mPropsMap.keys();
}


int CellTypes::saveCSV(const QString &fileName) const {
    QFile file(fileName);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return 0;
    }

    QTextStream out(&file);
    const QChar sep = ',';
    
    out << FIELD_ID << sep;
    out << FIELD_NAME << sep;
    out << FIELD_ISEXCITATORY << sep;
    out << FIELD_COLOR_DEND_R << sep;
    out << FIELD_COLOR_DEND_G << sep;
    out << FIELD_COLOR_DEND_B << sep;
    out << FIELD_COLOR_AXON_R << sep;
    out << FIELD_COLOR_AXON_G << sep;
    out << FIELD_COLOR_AXON_B << "\n";
    
    for (PropsMap::ConstIterator it = mPropsMap.begin(); it != mPropsMap.end(); ++it) {
        const Properties& props = it.value();
        out << it.key() << sep;
        out << props.name << sep;
        out << (props.isExcitatory ? "YES" : "NO") << sep;
        out << props.colorDendR << sep;
        out << props.colorDendG << sep;
        out << props.colorDendB << sep;
        out << props.colorAxonR << sep;
        out << props.colorAxonG << sep;
        out << props.colorAxonB << "\n";
    }

    return 1;
}


void CellTypes::loadCSV(const QString &fileName)
{
    QFile file(fileName);
    QTextStream(stdout) << "[*] Reading celltypes from " << fileName << "\n";

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString msg = QString("Error reading celltypes file. Could not open file %1").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    const QChar sep = ',';
    QTextStream in(&file);

    int lineCount = 1;
    QString line = in.readLine();
    if (line.isNull()) {
        const QString msg = QString("Error reading celltypes file %1. No content.").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    QStringList parts = line.split(sep);
    if (parts.size() != 9 ||
        parts[ 0] != FIELD_ID ||
        parts[ 1] != FIELD_NAME ||
        parts[ 2] != FIELD_ISEXCITATORY ||
        parts[ 3] != FIELD_COLOR_DEND_R ||
        parts[ 4] != FIELD_COLOR_DEND_G ||
        parts[ 5] != FIELD_COLOR_DEND_B ||
        parts[ 6] != FIELD_COLOR_AXON_R ||
        parts[ 7] != FIELD_COLOR_AXON_G ||
        parts[ 8] != FIELD_COLOR_AXON_B)
    {
        const QString msg = QString("Error reading celltypes file %1. Invalid column headers.").arg(fileName);
        throw std::runtime_error(qPrintable(msg));
    }

    line = in.readLine();
    lineCount += 1;

    while (!line.isNull()) {
        parts = line.split(sep);
        if (parts.size() != 9) {
            const QString msg = QString("Error reading celltypes file %1. Invalid columns.").arg(fileName);
            throw std::runtime_error(qPrintable(msg));
        }

        const int id =  parts[0].toInt();

        Properties props;
        props.name = parts[1];
        props.isExcitatory = (parts[2] == "YES");
        props.colorDendR = parts[3].toFloat();
        props.colorDendG = parts[4].toFloat();
        props.colorDendB = parts[5].toFloat();
        props.colorAxonR = parts[6].toFloat();
        props.colorAxonG = parts[7].toFloat();
        props.colorAxonB = parts[8].toFloat();

        mPropsMap.insert(id, props);

        line = in.readLine();
        lineCount += 1;
    }
    QTextStream(stdout) << "[*] Completed reading " <<  mPropsMap.size() << " celltypes.\n";
}
