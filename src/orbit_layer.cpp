#include "orbit_layer.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <cmath>
#include <numbers>
#include <algorithm>

namespace {
constexpr double rad = std::numbers::pi / 180.0;
double wrap(double value) { return value - std::floor((value + 180.0) / 360.0) * 360.0; }
void line(QVector<QPointF> &vertices, QPointF a, QPointF b, double width)
{
    const auto d = b - a;
    const double length = std::hypot(d.x(), d.y());
    if (length < 0.01) return;
    const QPointF n(-d.y() / length * width / 2, d.x() / length * width / 2);
    vertices << a + n << a - n << b + n << b + n << a - n << b - n;
}
void circle(QVector<QPointF> &vertices, QPointF center, double radius)
{
    for (int i = 0; i < 12; ++i) {
        const double a = i * std::numbers::pi / 6, b = (i + 1) * std::numbers::pi / 6;
        vertices << center << center + QPointF(std::cos(a), std::sin(a)) * radius << center + QPointF(std::cos(b), std::sin(b)) * radius;
    }
}
void append(QSGNode *root, const QVector<QPointF> &vertices, QColor color)
{
    if (vertices.isEmpty()) return;
    auto *node = new QSGGeometryNode;
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(vertices.size()));
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto *data = geometry->vertexDataAsPoint2D();
    for (qsizetype i = 0; i < vertices.size(); ++i) data[i].set(static_cast<float>(vertices[i].x()), static_cast<float>(vertices[i].y()));
    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);
    node->setGeometry(geometry); node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry); node->setFlag(QSGNode::OwnsMaterial);
    root->appendChildNode(node);
}
}

OrbitLayer::OrbitLayer(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &OrbitLayer::contentChanged, this, [this] { update(); });
}
void OrbitLayer::setMap(WorldMap *map)
{
    if (m_map) disconnect(m_map, nullptr, this, nullptr);
    m_map = map;
    if (map) connect(map, &WorldMap::viewChanged, this, [this] { emit contentChanged(); });
    emit mapChanged(); emit contentChanged();
}
QPointF OrbitLayer::project(double longitude, double latitude) const
{
    return {width() / 2 + wrap(longitude - m_map->centerLongitude()) * m_map->pixelsPerDegree(),
            height() / 2 - (latitude - m_map->centerLatitude()) * m_map->pixelsPerDegree()};
}
QPointF OrbitLayer::selectedPosition() const
{
    if (m_map) for (const auto &entry : m_markers) {
        const auto marker = entry.toMap();
        if (marker.value("id").toString() == m_selected) return project(marker.value("longitude").toDouble(), marker.value("latitude").toDouble());
    }
    return {-1000, -1000};
}
QString OrbitLayer::satelliteAt(double x, double y) const
{
    if (!m_map || !m_showSatellites) return {};
    double best = 10;
    QString result;
    for (const auto &entry : m_markers) {
        const auto marker = entry.toMap();
        const auto position = project(marker.value("longitude").toDouble(), marker.value("latitude").toDouble());
        const double distance = std::hypot(position.x() - x, position.y() - y);
        if (distance < best) { best = distance; result = marker.value("id").toString(); }
    }
    return result;
}
QSGNode *OrbitLayer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;
    auto *root = new QSGNode;
    if (!m_map) return root;
    const double worldWidth = m_map->pixelsPerDegree() * 360;
    QVector<QPointF> past, future, markers, selected, coverage;
    double dashOffset = 0;
    const auto segment = [&](QVector<QPointF> &vertices, QPointF a, QPointF b, double thickness, bool dashed = false) {
        if (b.x() - a.x() > worldWidth / 2) b.rx() -= worldWidth;
        if (b.x() - a.x() < -worldWidth / 2) b.rx() += worldWidth;
        const double length = std::hypot(b.x() - a.x(), b.y() - a.y());
        if (length < 0.01) return;
        const double phase = dashed ? dashOffset : 0;
        if (dashed) dashOffset = std::fmod(dashOffset + length, 9.0);
        const int copies = static_cast<int>(std::ceil(width() / worldWidth)) + 1;
        for (int copy = -copies; copy <= copies; ++copy) {
            const QPointF shift(copy * worldWidth, 0);
            if (!QRectF(a + shift, b + shift).normalized().adjusted(-3, -3, 3, 3).intersects(boundingRect())) continue;
            if (!dashed) line(vertices, a + shift, b + shift, thickness);
            else for (double d = -phase; d < length; d += 9) {
                const double begin = std::max(d, 0.0), end = std::min(d + 5, length);
                if (end > begin) line(vertices, a + shift + (b - a) * (begin / length), a + shift + (b - a) * (end / length), thickness);
            }
        }
    };
    for (qsizetype i = 1; i < m_trajectory.size(); ++i) {
        const auto a = m_trajectory[i - 1].toMap(), b = m_trajectory[i].toMap();
        const double ta = a.value("time").toDouble(), tb = b.value("time").toDouble();
        if (tb - ta > 31) continue;
        const auto pa = project(a.value("longitude").toDouble(), a.value("latitude").toDouble());
        const auto pb = project(b.value("longitude").toDouble(), b.value("latitude").toDouble());
        if (tb <= m_time) { if (m_showPast) segment(past, pa, pb, 1.5, true); }
        else if (ta >= m_time) { if (m_showFuture) segment(future, pa, pb, 1.6); }
        else {
            auto end = pb;
            if (end.x() - pa.x() > worldWidth / 2) end.rx() -= worldWidth;
            if (end.x() - pa.x() < -worldWidth / 2) end.rx() += worldWidth;
            const auto split = pa + (end - pa) * ((m_time - ta) / (tb - ta));
            if (m_showPast) segment(past, pa, split, 1.5, true);
            if (m_showFuture) segment(future, split, pb, 1.6);
        }
    }
    for (const auto &entry : m_markers) {
        const auto marker = entry.toMap();
        const double longitude = marker.value("longitude").toDouble(), latitude = marker.value("latitude").toDouble();
        const auto point = project(longitude, latitude);
        const bool current = marker.value("id").toString() == m_selected;
        if (m_showSatellites || current) {
            if (boundingRect().adjusted(-6, -6, 6, 6).contains(point)) circle(current ? selected : markers, point, current ? 4.5 : 2);
        }
        if (current && m_showCoverage) {
            const double radius = 6378.137, height = marker.value("altitude").toDouble(), e = m_minimumElevation * rad;
            const double angle = std::acos(std::clamp(radius / (radius + height) * std::cos(e), -1.0, 1.0)) - e;
            QPointF previous;
            for (int i = 0; i <= 180; ++i) {
                const double bearing = i * 2 * rad, lat = latitude * rad;
                const double y = std::asin(std::sin(lat) * std::cos(angle) + std::cos(lat) * std::sin(angle) * std::cos(bearing));
                const double x = longitude * rad + std::atan2(std::sin(bearing) * std::sin(angle) * std::cos(lat), std::cos(angle) - std::sin(lat) * std::sin(y));
                const auto next = project(x / rad, y / rad);
                if (i) segment(coverage, previous, next, 1.2);
                previous = next;
            }
        }
    }
    append(root, coverage, QColor("#9a79b8"));
    append(root, past, m_pastColor); append(root, future, m_futureColor);
    append(root, markers, m_markerColor); append(root, selected, m_futureColor);
    return root;
}
