#pragma once

#include "satellite_catalog.h"
#include <QAbstractListModel>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

class WatchlistModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
  public:
    explicit WatchlistModel(const SatelliteCatalog &catalog, QObject *parent = nullptr);
    enum Role { IdRole = Qt::UserRole + 1, NameRole, OriginalRole, ElevationRole, PredictionRole };
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QStringList &ids() const { return m_ids; }
    bool contains(qint64 id) const { return m_ids.contains(QString::number(id)); }
    bool setWatched(qint64 id, bool watched);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    int order() const { return m_order; }
    void setOrder(int order);
    int rowFor(qint64 id) const;
    void rebuild();
    void setElevations(QHash<qint64, double> elevations);
    void setPredictions(QHash<qint64, QString> summaries, QHash<qint64, double> nextTimes);
    void setHasObserver(bool hasObserver);
    void retranslate();

  signals:
    void changed();

  private:
    void sortRows();
    const SatelliteCatalog &m_catalog;
    QSettings m_settings;
    QStringList m_ids;
    QString m_search;
    QVector<int> m_rows;
    QHash<qint64, double> m_elevations;
    QHash<qint64, QString> m_summaries;
    QHash<qint64, double> m_nextTimes;
    int m_order = 0;
    bool m_hasObserver = false;
};
