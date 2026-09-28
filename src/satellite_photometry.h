#pragma once
#include "orbit_types.h"

namespace Orbit {
std::optional<double> apparentMagnitude(const State &state, double referenceMagnitude, double referencePhase);
}
