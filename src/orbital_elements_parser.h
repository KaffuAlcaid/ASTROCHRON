#pragma once
#include "orbit_types.h"

namespace Orbit {
QString formatEpoch(double unixSeconds);
std::optional<Satellite> fromOmm(const QJsonObject &object, QString &error);
QVector<Satellite> parse(const QByteArray &data, QString &error, int &skipped);
bool sameElements(const Satellite &a, const Satellite &b);
} // namespace Orbit
