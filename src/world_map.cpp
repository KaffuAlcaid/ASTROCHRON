#include "world_map.h"
#include "map_data.h"

#include <QFontMetricsF>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <algorithm>
#include <cmath>

namespace {
double wrapLongitude(double longitude)
{
    return longitude - 360.0 * std::floor((longitude + 180.0) / 360.0);
}

struct WorldNode : QSGNode {
    int detail = -1;
    int extent = 0;
    QVector<QSGTransformNode *> copies;
};

QSGGeometryNode *meshNode(const MapMesh &mesh, const QColor &color)
{
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),
                                    static_cast<int>(mesh.vertices.size()),
                                    static_cast<int>(mesh.indices.size()), QSGGeometry::UnsignedIntType);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto *points = geometry->vertexDataAsPoint2D();
    for (qsizetype i = 0; i < mesh.vertices.size(); ++i)
        points[i].set(static_cast<float>(mesh.vertices[i].x()), static_cast<float>(mesh.vertices[i].y()));
    std::copy(mesh.indices.begin(), mesh.indices.end(), geometry->indexDataAsUInt());
    auto *node = new QSGGeometryNode;
    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode *lineNode(const QVector<QPointF> &segments, const QColor &color)
{
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(segments.size()));
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    auto *points = geometry->vertexDataAsPoint2D();
    for (qsizetype i = 0; i < segments.size(); ++i)
        points[i].set(static_cast<float>(segments[i].x()), static_cast<float>(segments[i].y()));
    auto *node = new QSGGeometryNode;
    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QVector<QPointF> graticule()
{
    QVector<QPointF> points;
    for (int longitude = -180; longitude <= 180; longitude += 30)
        points << QPointF(longitude, -90) << QPointF(longitude, 90);
    for (int latitude = -90; latitude <= 90; latitude += 30)
        points << QPointF(-180, latitude) << QPointF(180, latitude);
    return points;
}
}

WorldMap::WorldMap(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setClip(true);
    worldMapData();
    connect(this, &WorldMap::layersChanged, this, [this] { m_rebuild = true; updateLabels(); update(); });
    connect(this, &WorldMap::appearanceChanged, this, [this] { m_rebuild = true; update(); });
    connect(this, &WorldMap::observerChanged, this, &WorldMap::updateView);
}

double WorldMap::pixelsPerDegree() const
{
    return std::max(0.001, std::min(width() / 360.0, height() / 180.0) * m_zoom);
}

QPointF WorldMap::screenPosition(double longitude, double latitude) const
{
    const auto scale = pixelsPerDegree();
    return {width() / 2.0 + wrapLongitude(longitude - m_longitude) * scale,
            height() / 2.0 + (m_latitude - latitude) * scale};
}

QPointF WorldMap::observerPosition() const { return screenPosition(m_observerLongitude, m_observerLatitude); }

QPointF WorldMap::coordinateAt(double x, double y) const
{
    const auto scale = pixelsPerDegree();
    return {wrapLongitude(m_longitude + (x - width() / 2.0) / scale),
            qBound(-90.0, m_latitude - (y - height() / 2.0) / scale, 90.0)};
}

void WorldMap::zoomAt(double factor, double x, double y)
{
    if (!std::isfinite(factor) || factor <= 0) return;
    const auto before = coordinateAt(x, y);
    m_zoom = qBound(1.0, m_zoom * factor, 12.0);
    const auto scale = pixelsPerDegree();
    m_longitude = before.x() - (x - width() / 2.0) / scale;
    m_latitude = before.y() + (y - height() / 2.0) / scale;
    updateView();
}

void WorldMap::panBy(double x, double y)
{
    m_longitude -= x / pixelsPerDegree();
    m_latitude += y / pixelsPerDegree();
    updateView();
}

void WorldMap::resetView()
{
    m_zoom = 1;
    m_longitude = 0;
    m_latitude = 0;
    updateView();
}

void WorldMap::centerOn(double longitude, double latitude)
{
    m_longitude = longitude;
    m_latitude = latitude;
    updateView();
}

void WorldMap::geometryChange(const QRectF &next, const QRectF &previous)
{
    QQuickItem::geometryChange(next, previous);
    if (next.size() != previous.size()) updateView();
}

void WorldMap::updateView()
{
    m_longitude = wrapLongitude(m_longitude);
    const double latitudeLimit = std::max(0.0, 90.0 - height() / (2.0 * pixelsPerDegree()));
    m_latitude = qBound(-latitudeLimit, m_latitude, latitudeLimit);
    updateLabels();
    emit viewChanged();
    update();
}

void WorldMap::updateLabels()
{
    QVariantList labels;
    if (m_showCities && width() > 0 && height() > 0) {
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(11);
        const QFontMetricsF metrics(font);
        QVector<QRectF> occupied;
        if (m_showStation) occupied.append(QRectF(observerPosition() - QPointF(9, 9), QSizeF(18, 18)));
        const int maxLabels = qBound(8, static_cast<int>(width() * height() / 8500.0), 100);
        const int maxRank = std::min(10, 3 + static_cast<int>(std::log2(m_zoom) * 2));
        for (const auto &city : worldMapData().cities) {
            if (city.rank > maxRank) continue;
            const auto point = screenPosition(city.longitude, city.latitude);
            if (point.x() < 5 || point.x() > width() - 5 || point.y() < 5 || point.y() > height() - 5) continue;
            const auto textWidth = metrics.horizontalAdvance(city.name);
            for (const auto offset : {QPointF(5, -16), QPointF(-textWidth - 5, -16), QPointF(5, 3)}) {
                const QRectF box(point + offset, QSizeF(textWidth, metrics.height()));
                if (!QRectF(3, 3, width() - 6, height() - 6).contains(box)) continue;
                if (std::any_of(occupied.begin(), occupied.end(), [&box](const auto &other) {
                    return other.intersects(box.adjusted(-4, -3, 4, 3));
                })) continue;
                occupied.append(box);
                labels.append(QVariantMap{{"name", city.name}, {"x", box.x()}, {"y", box.y()},
                                           {"pointX", point.x()}, {"pointY", point.y()}});
                break;
            }
            if (labels.size() >= maxLabels) break;
        }
    }
    m_labels = std::move(labels);
    emit cityLabelsChanged();
}

QSGNode *WorldMap::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<WorldNode *>(oldNode);
    const int detailIndex = m_zoom >= 2.5 ? 1 : 0;
    const int extent = static_cast<int>(std::ceil(width() / (720.0 * pixelsPerDegree()))) + 1;
    if (!node || m_rebuild || node->detail != detailIndex || node->extent != extent) {
        delete node;
        node = new WorldNode;
        node->detail = detailIndex;
        node->extent = extent;
        const auto &detail = worldMapData().details[detailIndex];
        for (int i = -extent; i <= extent; ++i) {
            auto *copy = new QSGTransformNode;
            copy->appendChildNode(meshNode(detail.land, m_landColor));
            if (m_showLakes) copy->appendChildNode(meshNode(detail.lakes, m_waterColor));
            if (m_showBorders) copy->appendChildNode(lineNode(detail.borderSegments, m_borderColor));
            if (m_showGrid) copy->appendChildNode(lineNode(graticule(), m_gridColor));
            node->appendChildNode(copy);
            node->copies.append(copy);
        }
        m_rebuild = false;
    }
    for (qsizetype i = 0; i < node->copies.size(); ++i) {
        QMatrix4x4 matrix;
        matrix.translate(static_cast<float>(width() / 2), static_cast<float>(height() / 2));
        matrix.scale(static_cast<float>(pixelsPerDegree()), static_cast<float>(pixelsPerDegree()));
        matrix.translate(static_cast<float>((i - extent) * 360 - m_longitude), static_cast<float>(m_latitude));
        node->copies[i]->setMatrix(matrix);
    }
    return node;
}
