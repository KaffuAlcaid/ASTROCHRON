#pragma once
#include "orbit_types.h"

namespace Orbit {
Sun sunAt(double unixSeconds);
ObserverGeometry observerGeometry(const Observer &observer);
PropagationContext propagationContext(double unixSeconds, const ObserverGeometry &observer);
// Position fills TEME vectors and geodetic coordinates; look adds observer geometry.
std::optional<State> position(const Satellite &satellite, const PropagationContext &context);
std::optional<State> look(const Satellite &satellite, const PropagationContext &context);
// Propagate also fills illumination, Sun elevation and phase angle.
std::optional<State> propagate(const Satellite &satellite, const PropagationContext &context);
} // namespace Orbit
