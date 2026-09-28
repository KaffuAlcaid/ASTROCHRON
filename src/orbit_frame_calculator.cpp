#include "orbit_frame_calculator.h"
#include "orbit_propagator.h"
#include "pass_predictor.h"
#include <algorithm>

namespace {
QVector<ShadowEvent> findShadowEvents(const Orbit::Satellite &satellite, const Orbit::Track &track,
                                      const Orbit::ObserverGeometry &geometry)
{
    QVector<ShadowEvent> events;
    for (qsizetype i = 1; i < track.samples.size(); ++i) {
        const auto &a = track.samples[i - 1];
        const auto &b = track.samples[i];
        if (b.time - a.time > Orbit::maximumSampleGapSeconds)
            continue;
        for (const int boundary : {1, 2}) {
            const bool before = a.illumination >= boundary;
            const bool after = b.illumination >= boundary;
            if (before == after)
                continue;
            double left = a.time, right = b.time;
            while (right - left > 0.5) {
                const double middle = (left + right) / 2;
                const auto state = Orbit::propagate(satellite, Orbit::propagationContext(middle, geometry));
                if (!state)
                    break;
                if ((state->illumination >= boundary) == before)
                    left = middle;
                else
                    right = middle;
            }
            events.append({(left + right) / 2, after, boundary == 2});
        }
    }
    std::sort(events.begin(), events.end(), [](const ShadowEvent &a, const ShadowEvent &b) { return a.time < b.time; });
    return events;
}
} // namespace

FrameResult calculateOrbitFrame(const FrameRequest &request)
{
    FrameResult result;
    result.revision = request.revision;
    result.time = request.time;
    result.referenceTime = request.referenceTime;
    result.calculatedTrack = request.calculateTrack;
    result.calculatedNavigation = request.calculateNavigation;
    result.hasHeight = request.hasHeight;
    const auto geometry = Orbit::observerGeometry(request.observer);
    const auto context = Orbit::propagationContext(request.time, geometry);
    const auto nextContext =
        request.interpolate ? Orbit::propagationContext(request.time + request.calculationInterval, geometry) : context;
    const auto nextNavigationContext = request.calculateNavigation && request.interpolateNavigation
                                           ? Orbit::propagationContext(request.time + 1, geometry)
                                           : context;

    for (const auto &satellite : request.navigationSatellites) {
        if (const auto state = Orbit::position(satellite, context)) {
            SatelliteMarker marker{satellite.number, *state, {}};
            if (request.interpolateNavigation)
                marker.nextPosition = Orbit::position(satellite, nextNavigationContext);
            result.navigationMarkers.append(marker);
        }
    }
    for (const auto &satellite : request.satellites) {
        const auto state = Orbit::propagate(satellite, context);
        if (!state)
            continue;
        result.elevations.insert(satellite.number, state->elevation);
        const bool inPreview = request.previewCandidates.contains(satellite.number) &&
                               (request.previewMode != 0 || state->elevation >= request.observer.minimumElevation);
        if (inPreview)
            ++result.previewCount;
        if (request.watchedIds.contains(satellite.number) || satellite.number == request.selectedId || inPreview) {
            SatelliteMarker marker{satellite.number, *state, {}};
            if (request.interpolate)
                marker.nextPosition = Orbit::position(satellite, nextContext);
            result.markers.append(marker);
        }
        if (satellite.number != request.selectedId)
            continue;
        result.observation = state;
        result.originalName = satellite.name;
        result.epochAgeDays = (request.time - satellite.epoch) / 86400.0;
        if (request.calculateTrack) {
            result.track = Orbit::track(satellite, request.referenceTime - 43200, request.referenceTime + 43200,
                                        request.observer, request.previousTrack);
            result.shadowEvents = findShadowEvents(satellite, result.track, geometry);
        }
    }
    return result;
}
