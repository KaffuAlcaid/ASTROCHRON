#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

struct MapMesh {
    QVector<QPointF> vertices;
    QVector<quint32> indices;
};

struct MapDetail {
    MapMesh land;
    MapMesh lakes;
    QVector<QPointF> borderSegments;
};

struct MapCity {
    QString displayName() const;
    QString name;
    QString originalName;
    QString timeZone;
    double longitude;
    double latitude;
    int rank;
    qint64 population;
};

struct MapData {
    QVector<MapDetail> details;
    QVector<MapCity> cities;
};

const MapData &worldMapData();
