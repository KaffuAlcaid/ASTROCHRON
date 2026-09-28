#include "pass_predictor.h"
#include "orbit_propagator.h"
#include <algorithm>
#include <cmath>

namespace Orbit {
namespace {
struct ElevationBracket {
    double left;
    double right;

    template <typename Evaluate> std::optional<double> refine(const Evaluate &at)
    {
        const double a = left + (right - left) / 3;
        const double b = right - (right - left) / 3;
        const auto first = at(a), second = at(b);
        if (!first || !second)
            return std::nullopt;
        if (first->elevation < second->elevation)
            left = a;
        else
            right = b;
        return std::max(first->elevation, second->elevation);
    }
};

std::optional<State> elevationPeak(const Satellite &satellite, double left, double right,
                                   const ObserverGeometry &observer, double resolutionSeconds)
{
    const auto at = [&](double time) { return propagate(satellite, propagationContext(time, observer)); };
    ElevationBracket bracket{left, right};
    while (bracket.right - bracket.left > resolutionSeconds) {
        if (!bracket.refine(at))
            return std::nullopt;
    }
    return at((bracket.left + bracket.right) / 2);
}
} // namespace

QSet<qint64> previewCandidates(const QVector<Satellite> &satellites, double time, const Observer &observer,
                               bool upcoming)
{
    QSet<qint64> candidates;
    const auto geometry = observerGeometry(observer);
    QVector<PropagationContext> contexts;
    const int steps = upcoming ? 30 : 0;
    for (int step = 0; step <= steps; ++step)
        contexts.append(propagationContext(time + step * trackSampleSeconds, geometry));
    for (const auto &satellite : satellites) {
        std::optional<State> previous;
        for (const auto &context : contexts) {
            const auto state = look(satellite, context);
            if (!state) {
                previous.reset();
                continue;
            }
            if (state->elevation >= observer.minimumElevation) {
                candidates.insert(satellite.number);
                break;
            }
            if (previous && previous->elevationRate > 0 && state->elevationRate < 0) {
                ElevationBracket bracket{previous->time, state->time};
                const auto at = [&](double sampleTime) {
                    return look(satellite, propagationContext(sampleTime, geometry));
                };
                for (int iteration = 0; iteration < 12; ++iteration) {
                    const auto elevation = bracket.refine(at);
                    if (!elevation)
                        break;
                    if (*elevation >= observer.minimumElevation) {
                        candidates.insert(satellite.number);
                        break;
                    }
                }
                if (candidates.contains(satellite.number))
                    break;
            }
            previous = state;
        }
    }
    return candidates;
}

Track track(const Satellite &satellite, double start, double end, const Observer &observer, const Track &previous)
{
    Track result;
    const auto geometry = observerGeometry(observer);
    const auto at = [&](double time) { return propagate(satellite, propagationContext(time, geometry)); };
    qsizetype cached = 0;
    const auto appendSample = [&](double time) {
        while (cached < previous.samples.size() && previous.samples[cached].time < time)
            ++cached;
        if (cached < previous.samples.size() && previous.samples[cached].time == time)
            result.samples.append(previous.samples[cached]);
        else if (const auto sample = at(time))
            result.samples.append(*sample);
    };
    // A fixed UTC grid lets successive time windows share their interior samples.
    appendSample(start);
    for (double time = (std::floor(start / trackSampleSeconds) + 1) * trackSampleSeconds; time < end;
         time += trackSampleSeconds)
        appendSample(time);
    if (end > start)
        appendSample(end);
    if (!result.samples.isEmpty())
        result.passes = predictPasses(satellite, start, end, observer, result.samples);
    // Keep short passes visible to drawing code even when they fall between grid samples.
    for (const auto &pass : result.passes) {
        result.samples.append(pass.rise);
        result.samples.append(pass.peak);
        result.samples.append(pass.set);
    }
    std::sort(result.samples.begin(), result.samples.end(),
              [](const State &a, const State &b) { return a.time < b.time; });
    result.samples.erase(std::unique(result.samples.begin(), result.samples.end(),
                                     [](const State &a, const State &b) { return a.time == b.time; }),
                         result.samples.end());
    return result;
}

QVector<Pass> predictPasses(const Satellite &satellite, double start, double end, const Observer &observer,
                            const QVector<State> &samples)
{
    QVector<Pass> result;
    if (end <= start)
        return result;
    const auto geometry = observerGeometry(observer);
    const auto at = [&](double time) { return propagate(satellite, propagationContext(time, geometry)); };
    QVector<State> points = samples;
    if (points.isEmpty()) {
        if (const auto point = at(start))
            points.append(*point);
        for (double time = (std::floor(start / trackSampleSeconds) + 1) * trackSampleSeconds; time < end;
             time += trackSampleSeconds)
            if (const auto point = at(time))
                points.append(*point);
        if (const auto point = at(end))
            points.append(*point);
    }
    const auto maximum = [&](double left, double right, double resolution) {
        return elevationPeak(satellite, left, right, geometry, resolution);
    };
    // Below-threshold endpoints can straddle a complete pass; bracket its peak by elevation rate.
    for (qsizetype i = points.size() - 1; i > 0; --i) {
        const auto &a = points[i - 1], &b = points[i];
        if (b.time - a.time <= maximumSampleGapSeconds && a.elevation < observer.minimumElevation &&
            b.elevation < observer.minimumElevation && a.elevationRate > 0 && b.elevationRate < 0) {
            const auto peak = maximum(a.time, b.time, 0.01);
            if (peak && peak->elevation >= observer.minimumElevation)
                points.insert(i, *peak);
        }
    }
    const auto crossing = [&](double left, double right, bool rising) {
        while (right - left > passBoundaryResolutionSeconds) {
            const double middle = (left + right) / 2;
            const auto sample = at(middle);
            if (!sample)
                break;
            if ((sample->elevation >= observer.minimumElevation) == rising)
                right = middle;
            else
                left = middle;
        }
        return at((left + right) / 2);
    };
    std::optional<State> rise;
    bool clipped = false;
    for (qsizetype i = 0; i < points.size(); ++i) {
        const auto &sample = points[i];
        if (i > 0 && sample.time - points[i - 1].time > maximumSampleGapSeconds)
            rise.reset();
        const bool above = sample.elevation >= observer.minimumElevation;
        if (above && !rise) {
            clipped = i == 0 || sample.time - points[i - 1].time > maximumSampleGapSeconds;
            rise = clipped ? std::optional(sample) : crossing(points[i - 1].time, sample.time, true);
        }
        if (rise && (!above || i + 1 == points.size())) {
            const bool endsAfter = above;
            const auto set = endsAfter ? std::optional(sample) : crossing(points[i - 1].time, sample.time, false);
            if (!set) {
                rise.reset();
                continue;
            }
            auto highest = *rise;
            for (const auto &point : points)
                if (point.time >= rise->time && point.time <= set->time && point.elevation > highest.elevation)
                    highest = point;
            const auto peak = maximum(std::max(rise->time, highest.time - trackSampleSeconds),
                                      std::min(set->time, highest.time + trackSampleSeconds), 0.5);
            if (peak) {
                Pass pass{*rise, peak->elevation > highest.elevation ? *peak : highest, *set, clipped, endsAfter, {}};
                std::optional<double> visibleStart;
                for (double time = rise->time; time <= set->time; time += 5) {
                    const auto point = at(time);
                    if (point && point->illumination == 0 && point->sunElevation <= -6) {
                        if (!visibleStart)
                            visibleStart = time;
                    } else if (visibleStart) {
                        pass.visibleIntervals.append({*visibleStart, time});
                        visibleStart.reset();
                    }
                }
                if (visibleStart)
                    pass.visibleIntervals.append({*visibleStart, set->time});
                result.append(pass);
            }
            rise.reset();
        }
    }
    return result;
}

} // namespace Orbit
