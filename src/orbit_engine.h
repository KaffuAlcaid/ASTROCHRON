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
std::optional<Satellite> fromOmm(const QJsonObject &object, QString &error);
QVector<Satellite> parse(const QByteArray &data, QString &error, int &skipped);
std::optional<State> propagate(const Satellite &satellite, double unixSeconds,
                               const Observer &observer, const Sun &sun);
Track track(const Satellite &satellite, double start, double end, const Observer &observer);
QString displayName(const Satellite &satellite);
QString illuminationName(int illumination);
QString directionName(double azimuth);
}
