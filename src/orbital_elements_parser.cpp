#include "orbital_elements_parser.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace Orbit {
namespace {
constexpr double pi = std::numbers::pi;
constexpr double rad = pi / 180.0;
constexpr double deg = 180.0 / pi;

double epochSeconds(QString text)
{
    static const QRegularExpression zone(QStringLiteral("(Z|[+-]\\d{2}:?\\d{2})$"));
    static const QRegularExpression fraction(QStringLiteral("\\.(\\d+)(?:Z|[+-]\\d{2}:?\\d{2})?$"));
    if (!zone.match(text).hasMatch())
        text += 'Z';
    auto date = QDateTime::fromString(text, Qt::ISODateWithMs).toUTC();
    if (!date.isValid())
        return std::numeric_limits<double>::quiet_NaN();
    const auto clock = date.time();
    date.setTime(QTime(clock.hour(), clock.minute(), clock.second()));
    const auto match = fraction.match(text);
    const double subsecond = match.hasMatch() ? (QStringLiteral("0.") + match.captured(1)).toDouble() : 0.0;
    return date.toMSecsSinceEpoch() / 1000.0 + subsecond;
}

} // namespace

QString formatEpoch(double seconds)
{
    const qint64 micros = static_cast<qint64>(std::llround(seconds * 1000000.0));
    qint64 whole = micros / 1000000, fraction = micros % 1000000;
    if (fraction < 0) {
        --whole;
        fraction += 1000000;
    }
    return QDateTime::fromSecsSinceEpoch(whole, QTimeZone::UTC).toString("yyyy-MM-dd'T'HH:mm:ss") +
           QStringLiteral(".%1").arg(fraction, 6, 10, QLatin1Char('0'));
}

namespace {
std::optional<Satellite> fromTle(const QString &name, const QString &first, const QString &second, QString &error)
{
    if (first.size() < 69 || second.size() < 69 || first.mid(2, 5) != second.mid(2, 5)) {
        error = QCoreApplication::translate("Orbit", "两行轨道根数的长度或卫星编号不匹配");
        return std::nullopt;
    }
    for (const auto &line : {first, second}) {
        int checksum = 0;
        for (qsizetype i = 0; i < 68; ++i) {
            if (line[i].isDigit())
                checksum += line[i].digitValue();
            else if (line[i] == '-')
                ++checksum;
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
    if (!validNumber) {
        error = QCoreApplication::translate("Orbit", "卫星编号格式无效");
        return std::nullopt;
    }
    for (const auto &[start, length] : {std::pair{8, 8}, {17, 8}, {26, 7}, {34, 8}, {43, 8}, {52, 11}}) {
        bool valid = false;
        second.mid(start, length).trimmed().toDouble(&valid);
        if (!valid) {
            error = QCoreApplication::translate("Orbit", "两行轨道根数含有无效数值");
            return std::nullopt;
        }
    }
    char line1[130]{}, line2[130]{};
    const auto bytes1 = first.left(69).toLatin1(), bytes2 = second.left(69).toLatin1();
    std::copy(bytes1.begin(), bytes1.end(), line1);
    std::copy(bytes2.begin(), bytes2.end(), line2);
    elsetrec record{};
    double start = 0, end = 0, step = 0;
    SGP4Funcs::twoline2rv(line1, line2, 'c', 'm', 'i', wgs72, start, end, step, record);
    if (record.error != 0) {
        error = QCoreApplication::translate("Orbit", "轨道根数初始化失败（%1）").arg(record.error);
        return std::nullopt;
    }
    QString international;
    const auto designator = first.mid(9, 8).trimmed();
    if (designator.size() >= 5) {
        const int year = designator.left(2).toInt();
        international = QStringLiteral("%1-%2").arg(year < 57 ? year + 2000 : year + 1900).arg(designator.mid(2));
    }
    QJsonObject object{{"OBJECT_NAME", name.isEmpty() ? QString::number(number) : name},
                       {"OBJECT_ID", international},
                       {"NORAD_CAT_ID", number},
                       {"EPOCH", formatEpoch((record.jdsatepoch - 2440587.5) * 86400.0 + record.jdsatepochF * 86400.0)},
                       {"MEAN_MOTION", record.no_kozai * 720.0 / pi},
                       {"ECCENTRICITY", record.ecco},
                       {"INCLINATION", record.inclo * deg},
                       {"RA_OF_ASC_NODE", record.nodeo * deg},
                       {"ARG_OF_PERICENTER", record.argpo * deg},
                       {"MEAN_ANOMALY", record.mo * deg},
                       {"BSTAR", record.bstar},
                       {"MEAN_MOTION_DOT", record.ndot * 1036800.0 / pi},
                       {"MEAN_MOTION_DDOT", record.nddot * 1492992000.0 / pi},
                       {"REV_AT_EPOCH", static_cast<qint64>(record.revnum)},
                       {"ELEMENT_SET_NO", static_cast<qint64>(record.elnum)},
                       {"CLASSIFICATION_TYPE", QString(QChar(record.classification))},
                       {"TLE_LINE1", first.left(69)},
                       {"TLE_LINE2", second.left(69)}};
    return fromOmm(object, error);
}
} // namespace

std::optional<Satellite> fromOmm(const QJsonObject &object, QString &error)
{
    for (const auto *key : {"NORAD_CAT_ID", "MEAN_MOTION", "ECCENTRICITY", "INCLINATION", "RA_OF_ASC_NODE",
                            "ARG_OF_PERICENTER", "MEAN_ANOMALY", "BSTAR"}) {
        if (!object.value(key).isDouble() || !std::isfinite(object.value(key).toDouble())) {
            error = QCoreApplication::translate("Orbit", "轨道数据缺少有效字段：%1").arg(QString::fromLatin1(key));
            return std::nullopt;
        }
    }
    if ((!object.value("TIME_SYSTEM").isUndefined() && object.value("TIME_SYSTEM").toString() != "UTC") ||
        (!object.value("MEAN_ELEMENT_THEORY").isUndefined() &&
         object.value("MEAN_ELEMENT_THEORY").toString() != "SGP4")) {
        error = QCoreApplication::translate("Orbit", "请选择采用协调世界时和 SGP4 的轨道根数");
        return std::nullopt;
    }
    if (!object.value("REF_FRAME").isUndefined() && object.value("REF_FRAME").toString() != "TEME") {
        error = QCoreApplication::translate("Orbit", "请选择采用 TEME 参考系的轨道根数");
        return std::nullopt;
    }
    Satellite satellite;
    satellite.number = object.value("NORAD_CAT_ID").toInteger();
    satellite.name = object.value("OBJECT_NAME").toString(QString::number(satellite.number));
    satellite.internationalId = object.value("OBJECT_ID").toString();
    satellite.epoch = epochSeconds(object.value("EPOCH").toString());
    satellite.elements = object;
    const double motion = object.value("MEAN_MOTION").toDouble(),
                 eccentricity = object.value("ECCENTRICITY").toDouble();
    const double inclination = object.value("INCLINATION").toDouble();
    if (!std::isfinite(satellite.epoch) || satellite.number < 1 || satellite.number > 999999999 || motion <= 0 ||
        eccentricity < 0 || eccentricity >= 1 || inclination < 0 || inclination > 180) {
        error = QCoreApplication::translate("Orbit", "轨道根数的历元、编号或数值范围无效");
        return std::nullopt;
    }
    // The upstream identifier buffer is five characters; full catalog IDs stay in our data model.
    const bool initialized =
        SGP4Funcs::sgp4init(wgs72, 'i', "00000", satellite.epoch / 86400.0 + 7306.0, object.value("BSTAR").toDouble(),
                            object.value("MEAN_MOTION_DOT").toDouble() * pi / 1036800.0,
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
    skipped = 0;
    error.clear();
    QHash<qint64, qsizetype> numbers;
    const auto accept = [&](std::optional<Satellite> satellite) {
        if (!satellite) {
            ++skipped;
            return;
        }
        const auto existing = numbers.constFind(satellite->number);
        if (existing == numbers.cend()) {
            numbers.insert(satellite->number, result.size());
            result.append(std::move(*satellite));
        } else if (satellite->epoch > result[*existing].epoch) {
            result[*existing] = std::move(*satellite);
        }
    };
    const auto trimmed = data.trimmed();
    if (trimmed.startsWith('[') || trimmed.startsWith('{')) {
        QJsonParseError jsonError;
        const auto document = QJsonDocument::fromJson(trimmed, &jsonError);
        if (jsonError.error != QJsonParseError::NoError) {
            error = QCoreApplication::translate("Orbit", "JSON 格式无效：%1").arg(jsonError.errorString());
            return {};
        }
        const QJsonArray array = document.isArray() ? document.array() : QJsonArray{document.object()};
        for (const auto value : array)
            accept(fromOmm(value.toObject(), error));
    } else {
        const auto lines = QString::fromUtf8(data).split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        QString name;
        for (qsizetype i = 0; i < lines.size(); ++i) {
            if (lines[i].startsWith("1 ")) {
                if (i + 1 >= lines.size() || !lines[i + 1].startsWith("2 ")) {
                    ++skipped;
                    error = QCoreApplication::translate("Orbit", "两行轨道根数缺少第二行");
                    continue;
                }
                accept(fromTle(name, lines[i], lines[i + 1], error));
                ++i;
                name.clear();
            } else if (!lines[i].startsWith('#'))
                name = lines[i].startsWith("0 ") ? lines[i].mid(2).trimmed() : lines[i].trimmed();
        }
    }
    if (result.isEmpty() && error.isEmpty() && trimmed != "[]")
        error = QCoreApplication::translate("Orbit", "文件中没有可用的轨道根数");
    return result;
}

bool sameElements(const Satellite &a, const Satellite &b)
{
    for (const auto *key : {"EPOCH", "MEAN_MOTION", "ECCENTRICITY", "INCLINATION", "RA_OF_ASC_NODE",
                            "ARG_OF_PERICENTER", "MEAN_ANOMALY", "BSTAR", "MEAN_MOTION_DOT", "MEAN_MOTION_DDOT"})
        if (a.elements.value(key) != b.elements.value(key))
            return false;
    return true;
}
} // namespace Orbit
