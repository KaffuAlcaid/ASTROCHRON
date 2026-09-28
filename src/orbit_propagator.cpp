#include "orbit_propagator.h"
#include "astronomy.h"
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Orbit {
namespace {
constexpr double pi = std::numbers::pi;
constexpr double rad = pi / 180.0;
constexpr double deg = 180.0 / pi;
constexpr double radius = 6378.137;
constexpr double flattening = 1.0 / 298.257223563;
constexpr double e2 = flattening * (2.0 - flattening);
constexpr double spin = 7.29211514670698e-5;

double norm(const Vector &v)
{
    return std::hypot(v[0], v[1], v[2]);
}
double dot(const Vector &a, const Vector &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
Vector subtract(const Vector &a, const Vector &b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vector rotate(const Vector &v, double angle)
{
    return {std::cos(angle) * v[0] + std::sin(angle) * v[1], -std::sin(angle) * v[0] + std::cos(angle) * v[1], v[2]};
}

Vector observerPosition(const Observer &observer)
{
    const double latitude = observer.latitude * rad;
    const double longitude = observer.longitude * rad;
    const double n = radius / std::sqrt(1.0 - e2 * std::pow(std::sin(latitude), 2));
    return {(n + observer.heightKm) * std::cos(latitude) * std::cos(longitude),
            (n + observer.heightKm) * std::cos(latitude) * std::sin(longitude),
            (n * (1.0 - e2) + observer.heightKm) * std::sin(latitude)};
}

std::array<double, 2> lookAngles(const Vector &delta, const ObserverGeometry &observer)
{
    const double east = -observer.sinLongitude * delta[0] + observer.cosLongitude * delta[1];
    const double north = -observer.sinLatitude * observer.cosLongitude * delta[0] -
                         observer.sinLatitude * observer.sinLongitude * delta[1] + observer.cosLatitude * delta[2];
    const double up = observer.cosLatitude * observer.cosLongitude * delta[0] +
                      observer.cosLatitude * observer.sinLongitude * delta[1] + observer.sinLatitude * delta[2];
    return {std::fmod(std::atan2(east, north) * deg + 360.0, 360.0), std::atan2(up, std::hypot(east, north)) * deg};
}

} // namespace
Sun sunAt(double seconds)
{
    const auto dateTime =
        QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(seconds * 1000)), QTimeZone::UTC);
    const auto date = dateTime.date();
    const auto clock = dateTime.time();
    auto time = Astronomy_MakeTime(date.year(), date.month(), date.day(), clock.hour(), clock.minute(),
                                   clock.second() + clock.msec() / 1000.0);
    const auto equator =
        Astronomy_RotateVector(Astronomy_Rotation_EQJ_EQD(&time), Astronomy_GeoVector(BODY_SUN, time, ABERRATION));
    const Vector earthFixed = rotate({equator.x * 149597870.7, equator.y * 149597870.7, equator.z * 149597870.7},
                                     Astronomy_SiderealTime(&time) * pi / 12.0);
    const double length = norm(earthFixed);
    return {earthFixed, {earthFixed[0] / length, earthFixed[1] / length, earthFixed[2] / length}};
}

ObserverGeometry observerGeometry(const Observer &observer)
{
    return {observer,
            observerPosition(observer),
            std::sin(observer.latitude * rad),
            std::cos(observer.latitude * rad),
            std::sin(observer.longitude * rad),
            std::cos(observer.longitude * rad)};
}

PropagationContext propagationContext(double seconds, const ObserverGeometry &observer)
{
    const double gmst = SGP4Funcs::gstime_SGP4(seconds / 86400.0 + 2440587.5);
    const auto sun = sunAt(seconds);
    return {seconds,
            std::sin(gmst),
            std::cos(gmst),
            observer,
            sun,
            lookAngles(subtract(sun.position, observer.position), observer)[1]};
}

std::optional<State> position(const Satellite &satellite, const PropagationContext &context)
{
    auto constants = satellite.constants;
    State state;
    state.time = context.time;
    if (!SGP4Funcs::sgp4(constants, (context.time - satellite.epoch) / 60.0, state.temePosition.data(),
                         state.temeVelocity.data()))
        return std::nullopt;
    state.earthPosition = {state.temePosition[0] * context.cosGmst + state.temePosition[1] * context.sinGmst,
                           -state.temePosition[0] * context.sinGmst + state.temePosition[1] * context.cosGmst,
                           state.temePosition[2]};
    const auto &[x, y, z] = state.earthPosition;
    const double horizontal = std::hypot(x, y);
    double latitude = std::atan2(z, horizontal * (1 - e2));
    for (int i = 0; i < 8; ++i) {
        const double n = radius / std::sqrt(1 - e2 * std::pow(std::sin(latitude), 2));
        const double next = std::atan2(z + e2 * n * std::sin(latitude), horizontal);
        if (std::abs(next - latitude) < 1e-13) {
            latitude = next;
            break;
        }
        latitude = next;
    }
    state.latitude = latitude * deg;
    state.longitude = std::atan2(y, x) * deg;
    state.altitude = horizontal < 1e-8 ? std::abs(z) - radius * (1 - flattening)
                                       : horizontal / std::cos(latitude) -
                                             radius / std::sqrt(1 - e2 * std::pow(std::sin(latitude), 2));
    return state;
}

std::optional<State> look(const Satellite &satellite, const PropagationContext &context)
{
    auto sample = position(satellite, context);
    if (!sample)
        return std::nullopt;
    auto &state = *sample;
    const auto &observer = context.observer;
    Vector velocity{state.temeVelocity[0] * context.cosGmst + state.temeVelocity[1] * context.sinGmst,
                    -state.temeVelocity[0] * context.sinGmst + state.temeVelocity[1] * context.cosGmst,
                    state.temeVelocity[2]};
    velocity[0] += spin * state.earthPosition[1];
    velocity[1] -= spin * state.earthPosition[0];
    state.speed = norm(state.temeVelocity);
    const auto delta = subtract(state.earthPosition, observer.position);
    const auto angles = lookAngles(delta, observer);
    state.azimuth = angles[0];
    state.elevation = angles[1];
    state.range = norm(delta);
    state.rangeRate = dot(delta, velocity) / state.range;
    const double upSpeed = velocity[0] * observer.cosLatitude * observer.cosLongitude +
                           velocity[1] * observer.cosLatitude * observer.sinLongitude +
                           velocity[2] * observer.sinLatitude;
    const double cosineElevation = std::cos(state.elevation * rad);
    if (std::abs(cosineElevation) > 1e-8)
        state.elevationRate =
            (upSpeed - state.rangeRate * std::sin(state.elevation * rad)) / (state.range * cosineElevation) * deg;
    return sample;
}

std::optional<State> propagate(const Satellite &satellite, const PropagationContext &context)
{
    auto sample = look(satellite, context);
    if (!sample)
        return std::nullopt;
    auto &state = *sample;
    const auto &sun = context.sun;
    const auto delta = subtract(state.earthPosition, context.observer.position);
    state.sunElevation = context.sunElevation;
    const auto satelliteToSun = subtract(sun.position, state.earthPosition);
    const double r = norm(state.earthPosition), d = norm(satelliteToSun);
    state.phaseAngle = std::acos(std::clamp(-dot(delta, satelliteToSun) / (state.range * d), -1.0, 1.0)) * deg;
    const double angle = std::acos(std::clamp(-dot(state.earthPosition, satelliteToSun) / (r * d), -1.0, 1.0));
    const double earthAngle = std::asin(std::min(1.0, radius / r)), sunAngle = std::asin(695700.0 / d);
    state.illumination = angle < earthAngle - sunAngle ? 2 : angle < earthAngle + sunAngle ? 1 : 0;
    return sample;
}

} // namespace Orbit
