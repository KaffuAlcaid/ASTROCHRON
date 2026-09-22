#pragma once

#include "world_map.h"
#include <QQuickItem>

class OrbitLayer : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(WorldMap *map READ map WRITE setMap NOTIFY mapChanged)
    Q_PROPERTY(QVariantList markers MEMBER m_markers NOTIFY contentChanged)
    Q_PROPERTY(QVariantList trajectory MEMBER m_trajectory NOTIFY contentChanged)
    Q_PROPERTY(QString selectedId MEMBER m_selected NOTIFY contentChanged)
    Q_PROPERTY(double time MEMBER m_time NOTIFY contentChanged)
    Q_PROPERTY(double minimumElevation MEMBER m_minimumElevation NOTIFY contentChanged)
    Q_PROPERTY(double highlightStart MEMBER m_highlightStart NOTIFY contentChanged)
    Q_PROPERTY(double highlightEnd MEMBER m_highlightEnd NOTIFY contentChanged)
    Q_PROPERTY(bool showPast MEMBER m_showPast NOTIFY contentChanged)
    Q_PROPERTY(bool showFuture MEMBER m_showFuture NOTIFY contentChanged)
    Q_PROPERTY(bool showCoverage MEMBER m_showCoverage NOTIFY contentChanged)
    Q_PROPERTY(bool showSatellites MEMBER m_showSatellites NOTIFY contentChanged)
    Q_PROPERTY(QColor pastColor MEMBER m_pastColor NOTIFY contentChanged)
    Q_PROPERTY(QColor futureColor MEMBER m_futureColor NOTIFY contentChanged)
    Q_PROPERTY(QColor markerColor MEMBER m_markerColor NOTIFY contentChanged)
    Q_PROPERTY(QPointF selectedPosition READ selectedPosition NOTIFY contentChanged)

public:
    explicit OrbitLayer(QQuickItem *parent = nullptr);
    WorldMap *map() const { return m_map; }
    void setMap(WorldMap *map);
    QPointF selectedPosition() const;
    Q_INVOKABLE QString satelliteAt(double x, double y) const;
signals:
    void mapChanged();
    void contentChanged();
protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
private:
    QPointF project(double longitude, double latitude) const;
    WorldMap *m_map = nullptr;
    QVariantList m_markers, m_trajectory;
    QString m_selected;
    double m_time = 0, m_minimumElevation = 10;
    double m_highlightStart = 0, m_highlightEnd = 0;
    bool m_showPast = true, m_showFuture = true, m_showCoverage = false, m_showSatellites = true;
    QColor m_pastColor{"#be814c"}, m_futureColor{"#087e6f"}, m_markerColor{"#5979a7"};
};
