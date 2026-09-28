#pragma once

#include "SGP4.h"
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <array>
#include <optional>

namespace Orbit {
using Vector = std::array<double, 3>;

struct Observer {
    // Angles are degrees; height is above the reference ellipsoid in km.
    double latitude = 0;
    double longitude = 0;
    double heightKm = 0;
    double minimumElevation = 10;
};

struct Sun {
    Vector position;
    Vector direction;
};

struct ObserverGeometry {
    Observer observer;
    Vector position;
    double sinLatitude, cosLatitude, sinLongitude, cosLongitude;
};

struct PropagationContext {
    double time, sinGmst, cosGmst;
    ObserverGeometry observer;
    Sun sun;
    double sunElevation;
};

struct State {
    // UTC Unix seconds, distances in km, speeds in km/s, angles in degrees.
    // elevationRate is degrees/s. Position vectors name their reference frame.
    double time = 0;
    Vector temePosition;
    Vector temeVelocity;
    Vector earthPosition;
    double latitude = 0;
    double longitude = 0;
    double altitude = 0;
    double speed = 0;
    double azimuth = 0;
    double elevation = 0;
    double elevationRate = 0;
    double range = 0;
    double rangeRate = 0;
    double sunElevation = 0;
    double phaseAngle = 0;
    int illumination = 0; // 0: sunlight, 1: penumbra, 2: umbra
};

struct Satellite {
    qint64 number = 0;
    QString name;
    QString internationalId;
    double epoch = 0; // UTC Unix seconds
    QJsonObject elements;
    elsetrec constants{};
};

struct Pass {
    State rise;
    State peak;
    State set;
    bool startsBeforeWindow = false;
    bool endsAfterWindow = false;
    QVector<std::array<double, 2>> visibleIntervals;
};

struct Track {
    QVector<State> samples;
    QVector<Pass> passes;
};

} // namespace Orbit
