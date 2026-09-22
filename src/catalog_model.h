#pragma once

#include "satellite_model.h"
#include <QAbstractListModel>

class CatalogModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(SatelliteModel *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QVariantList navigationGroups READ navigationGroups NOTIFY groupsChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY groupsChanged)
public:
    explicit CatalogModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    enum Role { KeyRole = Qt::UserRole + 1, NameRole, CountRole, GroupRole, ExpandedRole, WatchedRole, NumberRole, PrnRole, PlaneRole, NodeRole };
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    SatelliteModel *source() const { return m_source; }
    void setSource(SatelliteModel *source);
    QString search() const { return m_search; }
    void setSearch(const QString &text);
    QVariantList navigationGroups() const;
    int resultCount() const { return m_resultCount; }
    Q_INVOKABLE void toggleGroup(const QString &key);
    Q_INVOKABLE void revealGroup(const QString &key);
    static QString constellation(const Orbit::Satellite &satellite);
    static QString constellationName(const QString &key);
signals:
    void sourceChanged();
    void searchChanged();
    void groupsChanged();
private:
    struct Row { QString group; int satellite = -1; int count = 0; };
    void rebuild();
    SatelliteModel *m_source = nullptr;
    QString m_search;
    QSet<QString> m_expanded;
    QVector<Row> m_rows;
    QHash<QString, int> m_counts;
    int m_resultCount = 0;
};
