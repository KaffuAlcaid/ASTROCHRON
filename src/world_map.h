#pragma once

#include <QColor>
#include <QQuickItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class WorldMap : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(double centerLongitude READ centerLongitude NOTIFY viewChanged)
    Q_PROPERTY(double centerLatitude READ centerLatitude NOTIFY viewChanged)
    Q_PROPERTY(double pixelsPerDegree READ pixelsPerDegree NOTIFY viewChanged)
    Q_PROPERTY(QVariantList cityLabels READ cityLabels NOTIFY cityLabelsChanged)
    Q_PROPERTY(QPointF observerPosition READ observerPosition NOTIFY viewChanged)
    Q_PROPERTY(double observerLongitude MEMBER m_observerLongitude NOTIFY observerChanged)
    Q_PROPERTY(double observerLatitude MEMBER m_observerLatitude NOTIFY observerChanged)
    Q_PROPERTY(bool showLakes MEMBER m_showLakes NOTIFY layersChanged)
    Q_PROPERTY(bool showBorders MEMBER m_showBorders NOTIFY layersChanged)
    Q_PROPERTY(bool showGrid MEMBER m_showGrid NOTIFY layersChanged)
    Q_PROPERTY(bool showCities MEMBER m_showCities NOTIFY layersChanged)
    Q_PROPERTY(bool showStation MEMBER m_showStation NOTIFY layersChanged)
    Q_PROPERTY(QColor landColor MEMBER m_landColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor waterColor MEMBER m_waterColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor borderColor MEMBER m_borderColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY appearanceChanged)

public:
    explicit WorldMap(QQuickItem *parent = nullptr);
    double zoom() const { return m_zoom; }
    double centerLongitude() const { return m_longitude; }
    double centerLatitude() const { return m_latitude; }
    double pixelsPerDegree() const;
    QVariantList cityLabels() const { return m_labels; }
    QPointF observerPosition() const;
    Q_INVOKABLE void zoomAt(double factor, double x, double y);
    Q_INVOKABLE void panBy(double x, double y);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void centerOn(double longitude, double latitude);
    Q_INVOKABLE QPointF coordinateAt(double x, double y) const;
    Q_INVOKABLE void updateLabels();

signals:
    void viewChanged();
    void cityLabelsChanged();
    void observerChanged();
    void layersChanged();
    void appearanceChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    void updateView();
    QPointF screenPosition(double longitude, double latitude) const;
    double m_zoom = 1;
    double m_longitude = 0;
    double m_latitude = 0;
    double m_observerLongitude = 121.5654;
    double m_observerLatitude = 25.0330;
    bool m_showLakes = true;
    bool m_showBorders = true;
    bool m_showGrid = true;
    bool m_showCities = true;
    bool m_showStation = true;
    bool m_rebuild = true;
    QColor m_landColor = QColor("#526a70");
    QColor m_waterColor = QColor("#233033");
    QColor m_borderColor = QColor("#91a3a7");
    QColor m_gridColor = QColor("#466067");
    QVariantList m_labels;
};
