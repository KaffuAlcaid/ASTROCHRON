#pragma once

#include "orbit_types.h"
#include <QCoreApplication>
#include <QHash>
#include <QIODevice>
#include <QSqlDatabase>
#include <QUrl>
#include <functional>

struct PhotometryReference {
    double magnitude = 0;
    int phase = 90;
    QString source;
    bool manual = false;
    QString sourceDate;
    qint64 recordedAt = 0;
    QString internationalId;
};

class PhotometryCatalog {
    Q_DECLARE_TR_FUNCTIONS(PhotometryCatalog)
  public:
    explicit PhotometryCatalog(QSqlDatabase database);
    void load();
    const PhotometryReference *entry(const Orbit::Satellite *satellite) const;
    bool hasDefault(const Orbit::Satellite *satellite) const;
    bool hasOverride(qint64 id) const { return m_overrides.contains(id); }
    bool set(qint64 id, double magnitude, int phase, const QString &source, const QString &sourceDate);
    bool clear(const Orbit::Satellite *satellite);
    bool importMagnitudes(const QUrl &url, const QString &sourceDate);
    QString status() const { return m_status ? m_status() : QString(); }
    void resetStatus() { m_status = {}; }
    static QHash<qint64, PhotometryReference> readMagnitudes(QIODevice &input, const PhotometryReference &metadata,
                                                             int &skipped);

  private:
    QSqlDatabase m_database;
    QHash<qint64, PhotometryReference> m_overrides;
    QHash<qint64, PhotometryReference> m_defaults;
    std::function<QString()> m_status;
};
