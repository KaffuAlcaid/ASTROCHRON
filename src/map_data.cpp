#include "map_data.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

namespace {
MapMesh readMesh(const QJsonObject &object)
{
    MapMesh mesh;
    const auto vertices = object.value("vertices").toArray();
    mesh.vertices.reserve(vertices.size() / 2);
    for (qsizetype i = 0; i + 1 < vertices.size(); i += 2)
        mesh.vertices.append({vertices[i].toDouble(), vertices[i + 1].toDouble()});
    const auto indices = object.value("indices").toArray();
    mesh.indices.reserve(indices.size());
    for (const auto index : indices)
        mesh.indices.append(static_cast<quint32>(index.toInteger()));
    return mesh;
}

MapData loadMap()
{
    QFile file(QStringLiteral(":/maps/world-map.json"));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot open embedded map resource");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        qFatal("Cannot parse embedded map resource: %s", qPrintable(error.errorString()));

    const auto object = document.object();
    MapData result;
    for (const auto value : object.value("detailLevels").toArray()) {
        const auto detail = value.toObject();
        MapDetail item{readMesh(detail.value("land").toObject()),
                       readMesh(detail.value("lakes").toObject()), {}};
        for (const auto lineValue : detail.value("borders").toArray()) {
            const auto line = lineValue.toArray();
            for (qsizetype i = 0; i + 3 < line.size(); i += 2) {
                item.borderSegments.append({line[i].toDouble(), line[i + 1].toDouble()});
                item.borderSegments.append({line[i + 2].toDouble(), line[i + 3].toDouble()});
            }
        }
        result.details.append(std::move(item));
    }
    if (result.details.size() != 2)
        qFatal("Map resource must contain two detail levels");
    for (const auto value : object.value("cities").toArray()) {
        const auto city = value.toObject();
        result.cities.append({city.value("name").toString(), city.value("originalName").toString(), city.value("timeZone").toString(),
                              city.value("longitude").toDouble(), city.value("latitude").toDouble(),
                              city.value("rank").toInt(), city.value("population").toInteger()});
    }
    return result;
}
}

const MapData &worldMapData()
{
    static const MapData data = loadMap();
    return data;
}

QString MapCity::displayName() const
{
    return QLocale().language() == QLocale::Chinese || originalName.isEmpty() ? name : originalName;
}
