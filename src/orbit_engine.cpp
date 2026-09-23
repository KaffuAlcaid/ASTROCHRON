#include "orbit_engine.h"
#include "astronomy.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <limits>

namespace Orbit {
namespace {
constexpr double pi = std::numbers::pi;
constexpr double rad = pi / 180.0;
constexpr double deg = 180.0 / pi;
constexpr double radius = 6378.137;
constexpr double flattening = 1.0 / 298.257223563;
constexpr double e2 = flattening * (2.0 - flattening);
constexpr double spin = 7.29211514670698e-5;

double norm(const Vector &v) { return std::hypot(v[0], v[1], v[2]); }
double dot(const Vector &a, const Vector &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vector subtract(const Vector &a, const Vector &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vector rotate(const Vector &v, double angle)
{
    return {std::cos(angle) * v[0] + std::sin(angle) * v[1],
            -std::sin(angle) * v[0] + std::cos(angle) * v[1], v[2]};
}

Vector observerPosition(const Observer &observer)
{
    const double latitude = observer.latitude * rad;
    const double longitude = observer.longitude * rad;
    const double n = radius / std::sqrt(1.0 - e2 * std::pow(std::sin(latitude), 2));
    return {(n + observer.heightKm) * std::cos(latitude) * std::cos(longitude),
            (n + observer.heightKm) * std::cos(latitude) * std::sin(longitude),
            (n * (1.0 - e2) + observer.heightKm) * std::sin(latitude)};
}

std::array<double, 2> lookAngles(const Vector &delta, const ObserverGeometry &observer)
{
    const double east = -observer.sinLongitude * delta[0] + observer.cosLongitude * delta[1];
    const double north = -observer.sinLatitude * observer.cosLongitude * delta[0]
        - observer.sinLatitude * observer.sinLongitude * delta[1] + observer.cosLatitude * delta[2];
    const double up = observer.cosLatitude * observer.cosLongitude * delta[0]
        + observer.cosLatitude * observer.sinLongitude * delta[1] + observer.sinLatitude * delta[2];
    return {std::fmod(std::atan2(east, north) * deg + 360.0, 360.0),
            std::atan2(up, std::hypot(east, north)) * deg};
}

double epochSeconds(QString text)
{
    static const QRegularExpression zone(QStringLiteral("(Z|[+-]\\d{2}:?\\d{2})$"));
    static const QRegularExpression fraction(QStringLiteral("\\.(\\d+)(?:Z|[+-]\\d{2}:?\\d{2})?$"));
    if (!zone.match(text).hasMatch()) text += 'Z';
    auto date = QDateTime::fromString(text, Qt::ISODateWithMs).toUTC();
    if (!date.isValid()) return std::numeric_limits<double>::quiet_NaN();
    const auto clock = date.time();
    date.setTime(QTime(clock.hour(), clock.minute(), clock.second()));
    const auto match = fraction.match(text);
    const double subsecond = match.hasMatch() ? (QStringLiteral("0.") + match.captured(1)).toDouble() : 0.0;
    return date.toMSecsSinceEpoch() / 1000.0 + subsecond;
}

QString formatEpoch(double seconds)
{
    const qint64 micros = static_cast<qint64>(std::llround(seconds * 1000000.0));
    qint64 whole = micros / 1000000, fraction = micros % 1000000;
    if (fraction < 0) { --whole; fraction += 1000000; }
    return QDateTime::fromSecsSinceEpoch(whole, QTimeZone::UTC).toString("yyyy-MM-dd'T'HH:mm:ss")
        + QStringLiteral(".%1").arg(fraction, 6, 10, QLatin1Char('0'));
}

std::optional<Satellite> fromTle(const QString &name, const QString &first, const QString &second, QString &error)
{
    if (first.size() < 69 || second.size() < 69 || first.mid(2, 5) != second.mid(2, 5)) {
        error = QCoreApplication::translate("Orbit", "两行轨道根数的长度或卫星编号不匹配");
        return std::nullopt;
    }
    for (const auto &line : {first, second}) {
        int checksum = 0;
        for (qsizetype i = 0; i < 68; ++i) {
            if (line[i].isDigit()) checksum += line[i].digitValue();
            else if (line[i] == '-') ++checksum;
        }
        if (!line[68].isDigit() || checksum % 10 != line[68].digitValue()) {
            error = QCoreApplication::translate("Orbit", "两行轨道根数的校验位不匹配");
            return std::nullopt;
        }
    }
    const auto identifier = first.mid(2, 5).trimmed();
    bool validNumber = false;
    qint64 number = identifier.toLongLong(&validNumber);
    if (!validNumber && identifier.size() == 5) {
        const auto prefix = QStringLiteral("0123456789ABCDEFGHJKLMNPQRSTUVWXYZ").indexOf(identifier[0]);
        const auto suffix = identifier.mid(1).toInt(&validNumber);
        validNumber = validNumber && prefix >= 10;
        number = prefix * 10000 + suffix;
    }
    if (!validNumber) { error = QCoreApplication::translate("Orbit", "卫星编号格式无效"); return std::nullopt; }
    for (const auto &[start, length] : {std::pair{8, 8}, {17, 8}, {26, 7}, {34, 8}, {43, 8}, {52, 11}}) {
        bool valid = false;
        second.mid(start, length).trimmed().toDouble(&valid);
        if (!valid) { error = QCoreApplication::translate("Orbit", "两行轨道根数含有无效数值"); return std::nullopt; }
    }
    char line1[130]{}, line2[130]{};
    const auto bytes1 = first.left(69).toLatin1(), bytes2 = second.left(69).toLatin1();
    std::copy(bytes1.begin(), bytes1.end(), line1);
    std::copy(bytes2.begin(), bytes2.end(), line2);
    elsetrec record{};
    double start = 0, end = 0, step = 0;
    SGP4Funcs::twoline2rv(line1, line2, 'c', 'm', 'i', wgs72, start, end, step, record);
    if (record.error != 0) { error = QCoreApplication::translate("Orbit", "轨道根数初始化失败（%1）").arg(record.error); return std::nullopt; }
    QString international;
    const auto designator = first.mid(9, 8).trimmed();
    if (designator.size() >= 5) {
        const int year = designator.left(2).toInt();
        international = QStringLiteral("%1-%2").arg(year < 57 ? year + 2000 : year + 1900).arg(designator.mid(2));
    }
    QJsonObject object{{"OBJECT_NAME", name.isEmpty() ? QString::number(number) : name}, {"OBJECT_ID", international},
        {"NORAD_CAT_ID", number}, {"EPOCH", formatEpoch((record.jdsatepoch - 2440587.5) * 86400.0 + record.jdsatepochF * 86400.0)},
        {"MEAN_MOTION", record.no_kozai * 720.0 / pi}, {"ECCENTRICITY", record.ecco},
        {"INCLINATION", record.inclo * deg}, {"RA_OF_ASC_NODE", record.nodeo * deg},
        {"ARG_OF_PERICENTER", record.argpo * deg}, {"MEAN_ANOMALY", record.mo * deg},
        {"BSTAR", record.bstar}, {"MEAN_MOTION_DOT", record.ndot * 1036800.0 / pi},
        {"MEAN_MOTION_DDOT", record.nddot * 1492992000.0 / pi}, {"REV_AT_EPOCH", static_cast<qint64>(record.revnum)},
        {"ELEMENT_SET_NO", static_cast<qint64>(record.elnum)}, {"CLASSIFICATION_TYPE", QString(QChar(record.classification))},
        {"TLE_LINE1", first.left(69)}, {"TLE_LINE2", second.left(69)}};
    return fromOmm(object, error);
}
}

Sun sunAt(double seconds)
{
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(seconds * 1000)), QTimeZone::UTC);
    const auto date = dateTime.date(); const auto clock = dateTime.time();
    auto time = Astronomy_MakeTime(date.year(), date.month(), date.day(), clock.hour(), clock.minute(),
                                  clock.second() + clock.msec() / 1000.0);
    const auto equator = Astronomy_RotateVector(Astronomy_Rotation_EQJ_EQD(&time), Astronomy_GeoVector(BODY_SUN, time, ABERRATION));
    const Vector earthFixed = rotate({equator.x * 149597870.7, equator.y * 149597870.7, equator.z * 149597870.7},
                                     Astronomy_SiderealTime(&time) * pi / 12.0);
    const double length = norm(earthFixed);
    return {earthFixed, {earthFixed[0] / length, earthFixed[1] / length, earthFixed[2] / length}};
}

std::optional<Satellite> fromOmm(const QJsonObject &object, QString &error)
{
    for (const auto *key : {"NORAD_CAT_ID", "MEAN_MOTION", "ECCENTRICITY", "INCLINATION", "RA_OF_ASC_NODE", "ARG_OF_PERICENTER", "MEAN_ANOMALY", "BSTAR"}) {
        if (!object.value(key).isDouble() || !std::isfinite(object.value(key).toDouble())) {
            error = QCoreApplication::translate("Orbit", "轨道数据缺少有效字段：%1").arg(QString::fromLatin1(key));
            return std::nullopt;
        }
    }
    if ((!object.value("TIME_SYSTEM").isUndefined() && object.value("TIME_SYSTEM").toString() != "UTC") ||
        (!object.value("MEAN_ELEMENT_THEORY").isUndefined() && object.value("MEAN_ELEMENT_THEORY").toString() != "SGP4")) {
        error = QCoreApplication::translate("Orbit", "请选择采用协调世界时和 SGP4 的轨道根数");
        return std::nullopt;
    }
    Satellite satellite;
    satellite.number = object.value("NORAD_CAT_ID").toInteger();
    satellite.name = object.value("OBJECT_NAME").toString(QString::number(satellite.number));
    satellite.internationalId = object.value("OBJECT_ID").toString();
    satellite.epoch = epochSeconds(object.value("EPOCH").toString());
    satellite.elements = object;
    const double motion = object.value("MEAN_MOTION").toDouble(), eccentricity = object.value("ECCENTRICITY").toDouble();
    const double inclination = object.value("INCLINATION").toDouble();
    if (!std::isfinite(satellite.epoch) || satellite.number < 1 || satellite.number > 999999999 ||
        motion <= 0 || eccentricity < 0 || eccentricity >= 1 || inclination < 0 || inclination > 180) {
        error = QCoreApplication::translate("Orbit", "轨道根数的历元、编号或数值范围无效");
        return std::nullopt;
    }
    // The upstream identifier buffer is five characters; full catalog IDs stay in our data model.
    const bool initialized = SGP4Funcs::sgp4init(wgs72, 'i', "00000", satellite.epoch / 86400.0 + 7306.0,
        object.value("BSTAR").toDouble(), object.value("MEAN_MOTION_DOT").toDouble() * pi / 1036800.0,
        object.value("MEAN_MOTION_DDOT").toDouble() * pi / 1492992000.0, eccentricity,
        object.value("ARG_OF_PERICENTER").toDouble() * rad, inclination * rad,
        object.value("MEAN_ANOMALY").toDouble() * rad, motion * pi / 720.0,
        object.value("RA_OF_ASC_NODE").toDouble() * rad, satellite.constants);
    if (!initialized || satellite.constants.error) {
        error = QCoreApplication::translate("Orbit", "轨道根数初始化失败（%1）").arg(satellite.constants.error);
        return std::nullopt;
    }
    return satellite;
}

QVector<Satellite> parse(const QByteArray &data, QString &error, int &skipped)
{
    QVector<Satellite> result;
    skipped = 0; error.clear();
    QSet<qint64> numbers;
    const auto accept = [&](std::optional<Satellite> satellite) {
        if (!satellite) { ++skipped; return; }
        if (!numbers.contains(satellite->number)) { numbers.insert(satellite->number); result.append(std::move(*satellite)); }
    };
    const auto trimmed = data.trimmed();
    if (trimmed.startsWith('[') || trimmed.startsWith('{')) {
        QJsonParseError jsonError;
        const auto document = QJsonDocument::fromJson(trimmed, &jsonError);
        if (jsonError.error != QJsonParseError::NoError) { error = QCoreApplication::translate("Orbit", "JSON 格式无效：%1").arg(jsonError.errorString()); return {}; }
        const QJsonArray array = document.isArray() ? document.array() : QJsonArray{document.object()};
        for (const auto value : array) accept(fromOmm(value.toObject(), error));
    } else {
        const auto lines = QString::fromUtf8(data).split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        QString name;
        for (qsizetype i = 0; i < lines.size(); ++i) {
            if (lines[i].startsWith("1 ")) {
                if (i + 1 >= lines.size() || !lines[i + 1].startsWith("2 ")) { ++skipped; error = QCoreApplication::translate("Orbit", "两行轨道根数缺少第二行"); continue; }
                accept(fromTle(name, lines[i], lines[i + 1], error));
                ++i; name.clear();
            } else if (!lines[i].startsWith('#')) name = lines[i].startsWith("0 ") ? lines[i].mid(2).trimmed() : lines[i].trimmed();
        }
    }
    if (result.isEmpty() && error.isEmpty() && trimmed != "[]") error = QCoreApplication::translate("Orbit", "文件中没有可用的轨道根数");
    if (!result.isEmpty()) error.clear();
    return result;
}

ObserverGeometry observerGeometry(const Observer &observer)
{
    return {observer, observerPosition(observer), std::sin(observer.latitude * rad), std::cos(observer.latitude * rad),
        std::sin(observer.longitude * rad), std::cos(observer.longitude * rad)};
}

PropagationContext propagationContext(double seconds, const ObserverGeometry &observer)
{
    const double gmst = SGP4Funcs::gstime_SGP4(seconds / 86400.0 + 2440587.5);
    const auto sun = sunAt(seconds);
    return {seconds, std::sin(gmst), std::cos(gmst), observer, sun, lookAngles(subtract(sun.position, observer.position), observer)[1]};
}

std::optional<State> position(const Satellite &satellite, const PropagationContext &context)
{
    auto constants = satellite.constants;
    State state; state.time = context.time;
    if (!SGP4Funcs::sgp4(constants, (context.time - satellite.epoch) / 60.0, state.temePosition.data(), state.temeVelocity.data())) return std::nullopt;
    state.earthPosition = {state.temePosition[0] * context.cosGmst + state.temePosition[1] * context.sinGmst,
        -state.temePosition[0] * context.sinGmst + state.temePosition[1] * context.cosGmst, state.temePosition[2]};
    const auto &[x, y, z] = state.earthPosition;
    const double horizontal = std::hypot(x, y);
    double latitude = std::atan2(z, horizontal * (1 - e2));
    for (int i = 0; i < 8; ++i) {
        const double n = radius / std::sqrt(1 - e2 * std::pow(std::sin(latitude), 2));
        const double next = std::atan2(z + e2 * n * std::sin(latitude), horizontal);
        if (std::abs(next - latitude) < 1e-13) { latitude = next; break; }
        latitude = next;
    }
    state.latitude = latitude * deg;
    state.longitude = std::atan2(y, x) * deg;
    state.altitude = horizontal < 1e-8 ? std::abs(z) - radius * (1 - flattening)
        : horizontal / std::cos(latitude) - radius / std::sqrt(1 - e2 * std::pow(std::sin(latitude), 2));
    return state;
}

std::optional<State> look(const Satellite &satellite, const PropagationContext &context)
{
    auto sample = position(satellite, context);
    if (!sample) return std::nullopt;
    auto &state = *sample;
    const auto &observer = context.observer;
    Vector velocity{state.temeVelocity[0] * context.cosGmst + state.temeVelocity[1] * context.sinGmst,
        -state.temeVelocity[0] * context.sinGmst + state.temeVelocity[1] * context.cosGmst, state.temeVelocity[2]};
    velocity[0] += spin * state.earthPosition[1]; velocity[1] -= spin * state.earthPosition[0];
    state.speed = norm(state.temeVelocity);
    const auto delta = subtract(state.earthPosition, observer.position);
    const auto angles = lookAngles(delta, observer);
    state.azimuth = angles[0]; state.elevation = angles[1]; state.range = norm(delta);
    state.rangeRate = dot(delta, velocity) / state.range;
    const double upSpeed = velocity[0] * observer.cosLatitude * observer.cosLongitude
        + velocity[1] * observer.cosLatitude * observer.sinLongitude + velocity[2] * observer.sinLatitude;
    const double cosineElevation = std::cos(state.elevation * rad);
    if (std::abs(cosineElevation) > 1e-8)
        state.elevationRate = (upSpeed - state.rangeRate * std::sin(state.elevation * rad)) / (state.range * cosineElevation) * deg;
    return sample;
}

std::optional<State> propagate(const Satellite &satellite, const PropagationContext &context)
{
    auto sample = look(satellite, context);
    if (!sample) return std::nullopt;
    auto &state = *sample;
    const auto &sun = context.sun;
    const auto delta = subtract(state.earthPosition, context.observer.position);
    state.sunElevation = context.sunElevation;
    const auto satelliteToSun = subtract(sun.position, state.earthPosition);
    const double r = norm(state.earthPosition), d = norm(satelliteToSun);
    state.phaseAngle = std::acos(std::clamp(-dot(delta, satelliteToSun) / (state.range * d), -1.0, 1.0)) * deg;
    const double angle = std::acos(std::clamp(-dot(state.earthPosition, satelliteToSun) / (r * d), -1.0, 1.0));
    const double earthAngle = std::asin(std::min(1.0, radius / r)), sunAngle = std::asin(695700.0 / d);
    state.illumination = angle < earthAngle - sunAngle ? 2 : angle < earthAngle + sunAngle ? 1 : 0;
    return sample;
}

std::optional<double> apparentMagnitude(const State &state, double referenceMagnitude, double referencePhase)
{
    if (state.elevation <= 0 || state.illumination != 0 || state.range <= 0 || !std::isfinite(referenceMagnitude)
        || (referencePhase != 0 && referencePhase != 90)) return std::nullopt;
    const auto phaseFunction = [](double degrees) {
        const double angle = degrees * rad;
        return (std::sin(angle) + (pi - angle) * std::cos(angle)) / pi;
    };
    // Lambert sphere, normalized to the supplied phase at a range of 1000 km.
    const double brightness = phaseFunction(state.phaseAngle) / phaseFunction(referencePhase);
    if (brightness <= 1e-12) return std::nullopt;
    const double magnitude = referenceMagnitude + 5 * std::log10(state.range / 1000) - 2.5 * std::log10(brightness);
    return std::isfinite(magnitude) ? std::optional(magnitude) : std::nullopt;
}

Track track(const Satellite &satellite, double start, double end, const Observer &observer, const Track &previous)
{
    Track result;
    const auto geometry = observerGeometry(observer);
    const auto at = [&](double time) { return propagate(satellite, propagationContext(time, geometry)); };
    qsizetype cached = 0;
    const auto appendSample = [&](double time) {
        while (cached < previous.samples.size() && previous.samples[cached].time < time) ++cached;
        if (cached < previous.samples.size() && previous.samples[cached].time == time)
            result.samples.append(previous.samples[cached]);
        else if (const auto sample = at(time)) result.samples.append(*sample);
    };
    // A fixed UTC grid lets successive time windows share their interior samples.
    appendSample(start);
    for (double time = (std::floor(start / 30) + 1) * 30; time < end; time += 30) appendSample(time);
    if (end > start) appendSample(end);
    if (!result.samples.isEmpty()) result.passes = predictPasses(satellite, start, end, observer, result.samples);
    return result;
}

QVector<Pass> predictPasses(const Satellite &satellite, double start, double end, const Observer &observer, const QVector<State> &samples)
{
    QVector<Pass> result;
    if (end <= start) return result;
    const auto geometry = observerGeometry(observer);
    const auto at = [&](double time) { return propagate(satellite, propagationContext(time, geometry)); };
    QVector<State> calculated;
    if (samples.isEmpty()) {
        if (const auto point = at(start)) calculated.append(*point);
        for (double time = (std::floor(start / 30) + 1) * 30; time < end; time += 30)
            if (const auto point = at(time)) calculated.append(*point);
        if (const auto point = at(end)) calculated.append(*point);
    }
    const auto &points = samples.isEmpty() ? calculated : samples;
    const auto crossing = [&](double left, double right, bool rising) {
        while (right - left > 0.5) {
            const double middle = (left + right) / 2;
            const auto sample = at(middle);
            if (!sample) break;
            if ((sample->elevation >= observer.minimumElevation) == rising) right = middle;
            else left = middle;
        }
        return at((left + right) / 2);
    };
    std::optional<State> rise;
    bool clipped = false;
    for (qsizetype i = 0; i < points.size(); ++i) {
        const auto &sample = points[i];
        if (i > 0 && sample.time - points[i - 1].time > 31) rise.reset();
        const bool above = sample.elevation >= observer.minimumElevation;
        if (above && !rise) {
            clipped = i == 0 || sample.time - points[i - 1].time > 31;
            rise = clipped ? std::optional(sample) : crossing(points[i - 1].time, sample.time, true);
        }
        if (rise && (!above || i + 1 == points.size())) {
            const bool endsAfter = above;
            const auto set = endsAfter ? std::optional(sample) : crossing(points[i - 1].time, sample.time, false);
            if (!set) { rise.reset(); continue; }
            auto highest = *rise;
            for (const auto &point : points)
                if (point.time >= rise->time && point.time <= set->time && point.elevation > highest.elevation) highest = point;
            double left = std::max(rise->time, highest.time - 30), right = std::min(set->time, highest.time + 30);
            while (right - left > 0.5) {
                const double a = left + (right - left) / 3, b = right - (right - left) / 3;
                const auto first = at(a), second = at(b);
                if (!first || !second) break;
                if (first->elevation < second->elevation) left = a; else right = b;
            }
            const auto peak = at((left + right) / 2);
            if (peak) {
                Pass pass{*rise, *peak, *set, clipped, endsAfter, {}};
                std::optional<double> visibleStart;
                for (double time = rise->time; time <= set->time; time += 5) {
                    const auto point = at(time);
                    if (point && point->illumination == 0 && point->sunElevation <= -6) {
                        if (!visibleStart) visibleStart = time;
                    } else if (visibleStart) {
                        pass.visibleIntervals.append({*visibleStart, time});
                        visibleStart.reset();
                    }
                }
                if (visibleStart) pass.visibleIntervals.append({*visibleStart, set->time});
                result.append(pass);
            }
            rise.reset();
        }
    }
    return result;
}

QString displayName(const Satellite &satellite)
{
    if (satellite.number == 25544) return QCoreApplication::translate("Orbit", "国际空间站");
    if (satellite.number == 48274) return QCoreApplication::translate("Orbit", "天和核心舱（CSS）");
    if (satellite.number == 36086) return QCoreApplication::translate("Orbit", "探索号实验舱");
    if (satellite.number == 49044) return QCoreApplication::translate("Orbit", "科学号实验舱");
    if (satellite.number == 53239) return QCoreApplication::translate("Orbit", "问天实验舱");
    if (satellite.number == 54216) return QCoreApplication::translate("Orbit", "梦天实验舱");
    if (satellite.number == 20580) return QCoreApplication::translate("Orbit", "哈勃空间望远镜");
    return satellite.name;
}
QString illuminationName(int value)
{
    return value == 2 ? QCoreApplication::translate("Orbit", "地球阴影") : value == 1 ? QCoreApplication::translate("Orbit", "半影") : QCoreApplication::translate("Orbit", "阳光照射");
}
QString directionName(double azimuth)
{
    static const char *names[]{QT_TRANSLATE_NOOP("Orbit", "北"), QT_TRANSLATE_NOOP("Orbit", "东北"), QT_TRANSLATE_NOOP("Orbit", "东"), QT_TRANSLATE_NOOP("Orbit", "东南"),
        QT_TRANSLATE_NOOP("Orbit", "南"), QT_TRANSLATE_NOOP("Orbit", "西南"), QT_TRANSLATE_NOOP("Orbit", "西"), QT_TRANSLATE_NOOP("Orbit", "西北")};
    return QCoreApplication::translate("Orbit", names[static_cast<int>(std::lround(azimuth / 45.0)) % 8]);
}
}
