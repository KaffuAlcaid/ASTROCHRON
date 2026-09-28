#pragma once
#include "orbit_types.h"
#include <QSet>

namespace Orbit {
inline constexpr double trackSampleSeconds = 30;
inline constexpr double maximumSampleGapSeconds = trackSampleSeconds + 1;
inline constexpr double passBoundaryResolutionSeconds = 0.5;

Track track(const Satellite &satellite, double start, double end, const Observer &observer, const Track &previous = {});
QVector<Pass> predictPasses(const Satellite &satellite, double start, double end, const Observer &observer,
                            const QVector<State> &samples = {});
QSet<qint64> previewCandidates(const QVector<Satellite> &satellites, double time, const Observer &observer,
                               bool upcoming);
} // namespace Orbit
