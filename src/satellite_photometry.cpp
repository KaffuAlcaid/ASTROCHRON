#include "satellite_photometry.h"
#include <cmath>
#include <numbers>

namespace Orbit {
namespace {
constexpr double pi = std::numbers::pi;
constexpr double rad = pi / 180.0;
} // namespace
std::optional<double> apparentMagnitude(const State &state, double referenceMagnitude, double referencePhase)
{
    if (state.elevation <= 0 || state.illumination != 0 || state.range <= 0 || !std::isfinite(referenceMagnitude) ||
        (referencePhase != 0 && referencePhase != 90))
        return std::nullopt;
    const auto phaseFunction = [](double degrees) {
        const double angle = degrees * rad;
        return (std::sin(angle) + (pi - angle) * std::cos(angle)) / pi;
    };
    // Lambert sphere, normalized to the supplied phase at a range of 1000 km.
    const double brightness = phaseFunction(state.phaseAngle) / phaseFunction(referencePhase);
    if (brightness <= 1e-12)
        return std::nullopt;
    const double magnitude = referenceMagnitude + 5 * std::log10(state.range / 1000) - 2.5 * std::log10(brightness);
    return std::isfinite(magnitude) ? std::optional(magnitude) : std::nullopt;
}

} // namespace Orbit
