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
    double epoch = 0;
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

Sun sunAt(double unixSeconds);
QString formatEpoch(double unixSeconds);
ObserverGeometry observerGeometry(const Observer &observer);
PropagationContext propagationContext(double unixSeconds, const ObserverGeometry &observer);
std::optional<Satellite> fromOmm(const QJsonObject &object, QString &error);
QVector<Satellite> parse(const QByteArray &data, QString &error, int &skipped);
std::optional<State> position(const Satellite &satellite, const PropagationContext &context);
std::optional<State> look(const Satellite &satellite, const PropagationContext &context);
std::optional<State> propagate(const Satellite &satellite, const PropagationContext &context);
Track track(const Satellite &satellite, double start, double end, const Observer &observer, const Track &previous = {});
QVector<Pass> predictPasses(const Satellite &satellite, double start, double end, const Observer &observer, const QVector<State> &samples = {});
std::optional<double> apparentMagnitude(const State &state, double referenceMagnitude, double referencePhase);
QString displayName(const Satellite &satellite);
QString illuminationName(int illumination);
QString directionName(double azimuth);
}
