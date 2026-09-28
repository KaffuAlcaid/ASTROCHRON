#pragma once

#include "orbit_types.h"
#include <QHash>
#include <QSet>

struct FrameRequest {
    QVector<Orbit::Satellite> satellites;
    QVector<Orbit::Satellite> navigationSatellites;
    QSet<qint64> watchedIds;
    QSet<qint64> previewCandidates;
    qint64 selectedId = 0;
    quint64 revision = 0;
    double time = 0;
    double referenceTime = 0;
    Orbit::Observer observer;
    bool calculateTrack = false;
    bool calculateNavigation = false;
    Orbit::Track previousTrack;
    int previewMode = 0;
    double calculationInterval = 1;
    bool interpolate = false;
    bool interpolateNavigation = false;
    bool hasHeight = false;
};

struct SatelliteMarker {
    qint64 id = 0;
    Orbit::State state;
    std::optional<Orbit::State> nextPosition;
};

struct ShadowEvent {
    double time = 0;
    bool entering = false;
    bool umbra = false;
};

struct FrameResult {
    quint64 revision = 0;
    double time = 0;
    double referenceTime = 0;
    bool calculatedTrack = false;
    bool calculatedNavigation = false;
    bool hasHeight = false;
    QVector<SatelliteMarker> markers;
    QVector<SatelliteMarker> navigationMarkers;
    std::optional<Orbit::State> observation;
    QString originalName;
    double epochAgeDays = 0;
    Orbit::Track track;
    QVector<ShadowEvent> shadowEvents;
    QHash<qint64, double> elevations;
    int previewCount = 0;
};

FrameResult calculateOrbitFrame(const FrameRequest &request);
