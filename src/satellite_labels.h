#pragma once
#include "orbit_types.h"

namespace Orbit {
QString displayName(const Satellite &satellite);
QString illuminationName(int illumination);
QString directionName(double azimuth);
} // namespace Orbit
