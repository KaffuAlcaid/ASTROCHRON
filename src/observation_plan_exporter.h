#pragma once

#include "observation_plan.h"
#include <QCoreApplication>

enum class PlanFormat { Csv, Ics };

class ObservationPlanExporter {
    Q_DECLARE_TR_FUNCTIONS(ObservationPlanExporter)
  public:
    static QByteArray serialize(const ObservationPlan &plan, PlanFormat format, qint64 generatedAt);
};
