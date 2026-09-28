#pragma once

#include "orbit_types.h"
#include <QTimeZone>

struct ObservationPlan {
    Orbit::Satellite satellite;
    Orbit::Observer observer;
    QString observerName;
    QString source;
    QTimeZone zone;
    double start = 0;
    double end = 0;
    double height = 0;
    bool heightKnown = false;
    QVector<Orbit::Pass> passes;
};
