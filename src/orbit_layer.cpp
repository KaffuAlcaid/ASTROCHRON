#include "orbit_layer.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
constexpr double rad = std::numbers::pi / 180.0;
enum Layer { Coverage, Past, Future, SplitPast, SplitFuture, Markers, HighlightedPast, HighlightedFuture,
             SplitHighlightedPast, SplitHighlightedFuture, Selected, LayerCount };
constexpr std::array<int, 4> paths{Past, HighlightedPast, Future, HighlightedFuture};

struct Segment {
    QPointF a, b;
    double start, end, dashPhase;
    bool highlighted;
    std::array<int, 4> after;
};

struct OrbitNode : QSGNode {
    std::array<QSGGeometryNode *, LayerCount> layers{};
    std::array<QSGTransformNode *, LayerCount> transforms{};
    std::array<QVector<QPointF>, LayerCount> scratch;
    QVector<Segment> segments;
    std::array<int, 4> totals{};
    quint64 revision = std::numeric_limits<quint64>::max();
    double longitude = 0, latitude = 0, scale = 0, width = 0, height = 0, highlightStart = 0, highlightEnd = 0;

    OrbitNode() {
        for (int i = 0; i < LayerCount; ++i) {
            auto *node = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            geometry->setDrawingMode(QSGGeometry::DrawTriangles);
            const bool path = std::find(paths.begin(), paths.end(), i) != paths.end();
            geometry->setVertexDataPattern(path ? QSGGeometry::StaticPattern : QSGGeometry::DynamicPattern);
            node->setGeometry(geometry);
            node->setMaterial(new QSGFlatColorMaterial);
            node->setFlag(QSGNode::OwnsGeometry);
            node->setFlag(QSGNode::OwnsMaterial);
            auto *transform = new QSGTransformNode;
            transform->appendChildNode(node);
            appendChildNode(transform);
            transforms[i] = transform;
            layers[i] = node;
        }
    }
};

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
    static const auto unit = [] {
        std::array<QPointF, 12> result;
        for (int i = 0; i < 12; ++i) result[i] = {std::cos(i * std::numbers::pi / 6), std::sin(i * std::numbers::pi / 6)};
        return result;
    }();
    for (int i = 0; i < 12; ++i) vertices << center << center + unit[i] * radius << center + unit[(i + 1) % 12] * radius;
}

void segment(QVector<QPointF> &vertices, QPointF a, QPointF b, double thickness, bool dashed,
             double phase, double worldWidth, const QRectF &bounds)
{
    if (b.x() - a.x() > worldWidth / 2) b.rx() -= worldWidth;
    if (b.x() - a.x() < -worldWidth / 2) b.rx() += worldWidth;
    const double length = std::hypot(b.x() - a.x(), b.y() - a.y());
    if (length < 0.01) return;
    const int copies = static_cast<int>(std::ceil(bounds.width() / worldWidth)) + 1;
    for (int copy = -copies; copy <= copies; ++copy) {
        const QPointF shift(copy * worldWidth, 0);
        if (!QRectF(a + shift, b + shift).normalized().adjusted(-3, -3, 3, 3).intersects(bounds)) continue;
        if (!dashed) line(vertices, a + shift, b + shift, thickness);
        else for (double d = -phase; d < length; d += 9) {
            const double begin = std::max(d, 0.0), end = std::min(d + 5, length);
            if (end > begin) line(vertices, a + shift + (b - a) * (begin / length), a + shift + (b - a) * (end / length), thickness);
        }
    }
}

void setVertices(QSGGeometryNode *node, const QVector<QPointF> &vertices)
{
    auto *geometry = node->geometry();
    if (vertices.isEmpty() && geometry->vertexCount() == 0) return;
    if (geometry->vertexCount() != vertices.size()) geometry->allocate(static_cast<int>(vertices.size()));
    auto *data = geometry->vertexDataAsPoint2D();
    for (qsizetype i = 0; i < vertices.size(); ++i) data[i].set(static_cast<float>(vertices[i].x()), static_cast<float>(vertices[i].y()));
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
}

void setCount(QSGGeometryNode *node, int count)
{
    if (node->geometry()->vertexCount() == count) return;
    node->geometry()->setVertexCount(count);
    node->geometry()->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
}

void setColor(QSGGeometryNode *node, const QColor &color)
{
    auto *material = static_cast<QSGFlatColorMaterial *>(node->material());
    if (material->color() == color) return;
    material->setColor(color);
    node->markDirty(QSGNode::DirtyMaterial);
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

void OrbitLayer::setMarkers(const QVariantList &markers)
{
    if (m_markers == markers) return;
    m_markers = markers;
    m_points.clear(); m_pointIndices.clear();
    m_points.reserve(markers.size());
    for (const auto &entry : markers) {
        const auto marker = entry.toMap();
        Marker point{marker.value("id").toString(), marker.value("longitude").toDouble(), marker.value("latitude").toDouble(),
            marker.value("altitude").toDouble(), marker.value("time").toDouble(), 0, 0, 0, 0};
        point.nextTime = marker.value("nextTime", point.time).toDouble();
        point.nextLongitude = marker.value("nextLongitude", point.longitude).toDouble();
        point.nextLatitude = marker.value("nextLatitude", point.latitude).toDouble();
        point.nextAltitude = marker.value("nextAltitude", point.altitude).toDouble();
        m_pointIndices.insert(point.id, m_points.size());
        m_points.append(point);
    }
    emit contentChanged();
}

void OrbitLayer::setTrajectory(const QVariantList &trajectory)
{
    if (m_trajectory == trajectory) return;
    m_trajectory = trajectory;
    ++m_trajectoryRevision;
    emit contentChanged();
}

QPointF OrbitLayer::project(double longitude, double latitude) const
{
    return {width() / 2 + wrap(longitude - m_map->centerLongitude()) * m_map->pixelsPerDegree(),
            height() / 2 - (latitude - m_map->centerLatitude()) * m_map->pixelsPerDegree()};
}

double OrbitLayer::markerFraction(const Marker &marker) const
{
    return marker.nextTime > marker.time ? std::clamp((m_time - marker.time) / (marker.nextTime - marker.time), 0.0, 1.0) : 0;
}

QPointF OrbitLayer::markerCoordinate(const Marker &marker) const
{
    const double f = markerFraction(marker);
    return {wrap(marker.longitude + wrap(marker.nextLongitude - marker.longitude) * f),
        marker.latitude + (marker.nextLatitude - marker.latitude) * f};
}

QPointF OrbitLayer::selectedPosition() const
{
    const auto index = m_pointIndices.value(m_selected, -1);
    if (m_map && index >= 0) {
        const auto coordinate = markerCoordinate(m_points[index]);
        return project(coordinate.x(), coordinate.y());
    }
    return {-1000, -1000};
}

QPointF OrbitLayer::selectedCoordinate(double time) const
{
    const auto index = m_pointIndices.value(m_selected, -1);
    if (index < 0) return {qQNaN(), qQNaN()};
    const auto &marker = m_points[index];
    const double f = marker.nextTime > marker.time ? std::clamp((time - marker.time) / (marker.nextTime - marker.time), 0.0, 1.0) : 0;
    return {wrap(marker.longitude + wrap(marker.nextLongitude - marker.longitude) * f),
        marker.latitude + (marker.nextLatitude - marker.latitude) * f};
}

QString OrbitLayer::satelliteAt(double x, double y) const
{
    if (!m_map || !m_showSatellites) return {};
    double best = 10;
    QString result;
    for (const auto &marker : m_points) {
        const auto coordinate = markerCoordinate(marker);
        const auto point = project(coordinate.x(), coordinate.y());
        const double distance = std::hypot(point.x() - x, point.y() - y);
        if (distance < best) { best = distance; result = marker.id; }
    }
    return result;
}

QSGNode *OrbitLayer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *root = oldNode ? static_cast<OrbitNode *>(oldNode) : new OrbitNode;
    if (!m_map || m_map->pixelsPerDegree() <= 0) {
        for (auto *node : root->layers) setCount(node, 0);
        return root;
    }
    const double scale = m_map->pixelsPerDegree(), worldWidth = scale * 360;
    const auto bounds = boundingRect();
    auto pastColor = m_pastColor, futureColor = m_futureColor, markerColor = m_markerColor;
    pastColor.setAlphaF(0.5f); futureColor.setAlphaF(0.55f); markerColor.setAlphaF(0.4f);
    setColor(root->layers[Coverage], QColor("#509a79b8"));
    setColor(root->layers[Past], pastColor); setColor(root->layers[Future], futureColor);
    setColor(root->layers[SplitPast], pastColor); setColor(root->layers[SplitFuture], futureColor);
    setColor(root->layers[Markers], markerColor); setColor(root->layers[Selected], m_futureColor);
    setColor(root->layers[HighlightedPast], m_pastColor); setColor(root->layers[HighlightedFuture], m_futureColor);
    setColor(root->layers[SplitHighlightedPast], m_pastColor); setColor(root->layers[SplitHighlightedFuture], m_futureColor);

    auto offset = QPointF(-wrap(m_map->centerLongitude() - root->longitude) * scale,
        (m_map->centerLatitude() - root->latitude) * scale);
    const auto pathBounds = bounds.adjusted(-256, -256, 256, 256);
    if (root->revision != m_trajectoryRevision || std::abs(offset.x()) > 128 || std::abs(offset.y()) > 128
        || root->scale != scale || root->width != width() || root->height != height()
        || root->highlightStart != m_highlightStart || root->highlightEnd != m_highlightEnd) {
        root->segments.clear(); root->totals = {};
        std::array<QVector<QPointF>, 4> vertices;
        double dash = 0;
        for (qsizetype i = 1; i < m_trajectory.size(); ++i) {
            const auto a = m_trajectory[i - 1].toMap(), b = m_trajectory[i].toMap();
            const double ta = a.value("time").toDouble(), tb = b.value("time").toDouble();
            if (tb <= ta || tb - ta > 31) continue;
            const auto pa = project(a.value("longitude").toDouble(), a.value("latitude").toDouble());
            auto pb = project(b.value("longitude").toDouble(), b.value("latitude").toDouble());
            if (pb.x() - pa.x() > worldWidth / 2) pb.rx() -= worldWidth;
            if (pb.x() - pa.x() < -worldWidth / 2) pb.rx() += worldWidth;
            const bool highlighted = (ta + tb) / 2 >= m_highlightStart && (ta + tb) / 2 <= m_highlightEnd;
            Segment entry{pa, pb, ta, tb, dash, highlighted, {}};
            const int past = highlighted ? 1 : 0, future = highlighted ? 3 : 2;
            segment(vertices[past], pa, pb, highlighted ? 1.5 : 1.25, true, dash, worldWidth, pathBounds);
            segment(vertices[future], pa, pb, highlighted ? 2.5 : 2, false, 0, worldWidth, pathBounds);
            const double length = std::hypot(pb.x() - pa.x(), pb.y() - pa.y());
            if (length >= 0.01) dash = std::fmod(dash + length, 9.0);
            for (int j = 0; j < 4; ++j) root->totals[j] = static_cast<int>(vertices[j].size());
            entry.after = root->totals;
            root->segments.append(entry);
        }
        // Reversed future triangles allow both time ranges to use a vertex prefix.
        for (int i : {2, 3}) {
            auto &v = vertices[i];
            for (qsizetype a = 0, b = v.size() - 3; a < b; a += 3, b -= 3)
                for (int k = 0; k < 3; ++k) std::swap(v[a + k], v[b + k]);
        }
        for (int i = 0; i < 4; ++i) setVertices(root->layers[paths[i]], vertices[i]);
        root->revision = m_trajectoryRevision;
        root->longitude = m_map->centerLongitude(); root->latitude = m_map->centerLatitude(); root->scale = scale;
        root->width = width(); root->height = height(); root->highlightStart = m_highlightStart; root->highlightEnd = m_highlightEnd;
        offset = {};
    }
    QMatrix4x4 translation;
    translation.translate(static_cast<float>(offset.x()), static_cast<float>(offset.y()));
    for (int layer : {Past, Future, SplitPast, SplitFuture, HighlightedPast, HighlightedFuture, SplitHighlightedPast, SplitHighlightedFuture})
        root->transforms[layer]->setMatrix(translation);

    const auto active = std::upper_bound(root->segments.cbegin(), root->segments.cend(), m_time,
        [](double time, const Segment &entry) { return time < entry.end; });
    const auto ended = active == root->segments.cbegin() ? std::array<int, 4>{} : (active - 1)->after;
    auto excluded = ended;
    auto &splitPast = root->scratch[SplitPast], &splitFuture = root->scratch[SplitFuture];
    splitPast.clear(); splitFuture.clear();
    if (active != root->segments.cend() && m_time > active->start) {
        excluded = active->after;
        const auto split = active->a + (active->b - active->a) * ((m_time - active->start) / (active->end - active->start));
        if (m_showPast) segment(splitPast, active->a, split, active->highlighted ? 1.5 : 1.25, true, active->dashPhase, worldWidth, pathBounds);
        if (m_showFuture) segment(splitFuture, split, active->b, active->highlighted ? 2.5 : 2, false, 0, worldWidth, pathBounds);
    }
    setCount(root->layers[Past], m_showPast ? ended[0] : 0);
    setCount(root->layers[HighlightedPast], m_showPast ? ended[1] : 0);
    setCount(root->layers[Future], m_showFuture ? root->totals[2] - excluded[2] : 0);
    setCount(root->layers[HighlightedFuture], m_showFuture ? root->totals[3] - excluded[3] : 0);
    const bool splitHighlighted = active != root->segments.cend() && active->highlighted;
    setVertices(root->layers[splitHighlighted ? SplitHighlightedPast : SplitPast], splitPast);
    setVertices(root->layers[splitHighlighted ? SplitHighlightedFuture : SplitFuture], splitFuture);
    setCount(root->layers[splitHighlighted ? SplitPast : SplitHighlightedPast], 0);
    setCount(root->layers[splitHighlighted ? SplitFuture : SplitHighlightedFuture], 0);

    auto &markers = root->scratch[Markers], &selected = root->scratch[Selected], &coverage = root->scratch[Coverage];
    markers.clear(); selected.clear(); coverage.clear();
    markers.reserve(m_points.size() * 36);
    for (const auto &marker : m_points) {
        const auto coordinate = markerCoordinate(marker);
        const auto point = project(coordinate.x(), coordinate.y());
        const bool current = marker.id == m_selected;
        if ((m_showSatellites || current) && bounds.adjusted(-6, -6, 6, 6).contains(point))
            circle(current ? selected : markers, point, current ? 4.5 : 2);
        if (current && m_showCoverage) {
            const double height = marker.altitude + (marker.nextAltitude - marker.altitude) * markerFraction(marker);
            const double radius = 6378.137, e = m_minimumElevation * rad, lat = coordinate.y() * rad;
            const double angle = std::acos(std::clamp(radius / (radius + height) * std::cos(e), -1.0, 1.0)) - e;
            const double sinLat = std::sin(lat), cosLat = std::cos(lat), sinAngle = std::sin(angle), cosAngle = std::cos(angle);
            QPointF previous;
            for (int i = 0; i <= 180; ++i) {
                const double bearing = i * 2 * rad;
                const double y = std::asin(sinLat * cosAngle + cosLat * sinAngle * std::cos(bearing));
                const double x = coordinate.x() * rad + std::atan2(std::sin(bearing) * sinAngle * cosLat, cosAngle - sinLat * std::sin(y));
                const auto next = project(x / rad, y / rad);
                if (i) segment(coverage, previous, next, 1.2, false, 0, worldWidth, bounds);
                previous = next;
            }
        }
    }
    setVertices(root->layers[Markers], markers); setVertices(root->layers[Selected], selected); setVertices(root->layers[Coverage], coverage);
    return root;
}
