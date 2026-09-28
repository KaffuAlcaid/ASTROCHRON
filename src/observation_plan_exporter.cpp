#include "observation_plan_exporter.h"
#include "orbital_elements_parser.h"
#include "satellite_labels.h"
#include <QDateTime>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
QString number(double value, int decimals = 2)
{
    return QString::number(value, 'f', decimals);
}
} // namespace

QByteArray ObservationPlanExporter::serialize(const ObservationPlan &plan, PlanFormat format, qint64 generatedAt)
{
    const auto iso = [](double time, const QTimeZone &zone) {
        return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(std::floor(time)), zone).toString(Qt::ISODate);
    };
    const auto utc = [](double time) {
        return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(std::floor(time)), QTimeZone::UTC)
            .toString("yyyyMMdd'T'HHmmss'Z'");
    };
    QByteArray output;
    const auto csvRow = [&](const QStringList &values) {
        QStringList escaped;
        for (auto value : values) {
            value.replace('"', "\"\"");
            escaped.append('"' + value + '"');
        }
        output += escaped.join(',').toUtf8() + "\r\n";
    };
    const auto icsText = [](QString value) {
        return value.replace("\r\n", "\n")
            .replace('\r', '\n')
            .replace("\\", "\\\\")
            .replace("\n", "\\n")
            .replace(";", "\\;")
            .replace(",", "\\,");
    };
    const auto icsLine = [&](const QString &line) {
        const auto bytes = line.toUtf8();
        qsizetype offset = 0;
        while (offset < bytes.size()) {
            qsizetype count = std::min<qsizetype>(offset ? 74 : 75, bytes.size() - offset);
            while (offset + count < bytes.size() && (static_cast<unsigned char>(bytes[offset + count]) & 0xc0) == 0x80)
                --count;
            if (offset)
                output += ' ';
            output += QByteArrayView(bytes).sliced(offset, count);
            output += "\r\n";
            offset += count;
        }
    };
    const auto name = Orbit::displayName(plan.satellite);
    const auto epoch = Orbit::formatEpoch(plan.satellite.epoch) + 'Z';
    const auto zone = QString::fromUtf8(plan.zone.id());
    const auto location =
        tr("%1（%2°, %3°）")
            .arg(plan.observerName, number(plan.observer.latitude, 6), number(plan.observer.longitude, 6));
    if (format == PlanFormat::Csv) {
        output = QByteArray::fromHex("efbbbf");
        csvRow({tr("卫星"),
                "NORAD ID",
                "COSPAR",
                tr("开始（UTC）"),
                tr("最高点（UTC）"),
                tr("结束（UTC）"),
                tr("最高高度角（°）"),
                tr("持续时间（s）"),
                tr("开始方位角（°）"),
                tr("结束方位角（°）"),
                tr("光学条件"),
                tr("光学区间（UTC）"),
                tr("轨道历元（UTC）"),
                tr("观测地点"),
                tr("纬度（°）"),
                tr("经度（°）"),
                tr("海拔（m）"),
                tr("最低高度角（°）"),
                tr("时区"),
                tr("开始（当地时间）"),
                tr("最高点（当地时间）"),
                tr("结束（当地时间）"),
                tr("起点截断"),
                tr("终点截断"),
                tr("数据来源")});
    } else {
        icsLine("BEGIN:VCALENDAR");
        icsLine("VERSION:2.0");
        icsLine("PRODID:-//ASTROCHRON//Observation Plan//EN");
        icsLine("CALSCALE:GREGORIAN");
    }
    for (const auto &pass : plan.passes) {
        QStringList optical;
        for (const auto &interval : pass.visibleIntervals)
            optical.append(iso(interval[0], QTimeZone::UTC) + " / " + iso(interval[1], QTimeZone::UTC));
        if (format == PlanFormat::Csv) {
            csvRow({name,
                    QString::number(plan.satellite.number),
                    plan.satellite.internationalId,
                    iso(pass.rise.time, QTimeZone::UTC),
                    iso(pass.peak.time, QTimeZone::UTC),
                    iso(pass.set.time, QTimeZone::UTC),
                    number(pass.peak.elevation, 1),
                    number(pass.set.time - pass.rise.time, 1),
                    number(pass.rise.azimuth, 1),
                    number(pass.set.azimuth, 1),
                    optical.isEmpty() ? tr("条件欠佳") : tr("具备光学条件"),
                    optical.join("; "),
                    epoch,
                    plan.observerName,
                    number(plan.observer.latitude, 6),
                    number(plan.observer.longitude, 6),
                    plan.heightKnown ? number(plan.height, 1) : QString(),
                    number(plan.observer.minimumElevation, 1),
                    zone,
                    iso(pass.rise.time, plan.zone),
                    iso(pass.peak.time, plan.zone),
                    iso(pass.set.time, plan.zone),
                    pass.startsBeforeWindow ? "1" : "0",
                    pass.endsAfterWindow ? "1" : "0",
                    plan.source});
        } else {
            const bool partial = pass.startsBeforeWindow || pass.endsAfterWindow;
            const auto title =
                partial ? tr("ASTROCHRON · %1观测时段（时段内最高 %2°）").arg(name, number(pass.peak.elevation, 1))
                        : tr("ASTROCHRON · %1过境（峰值 %2°）").arg(name, number(pass.peak.elevation, 1));
            const auto identity = QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
                                      .arg(plan.satellite.number)
                                      .arg(epoch, number(plan.observer.latitude, 8), number(plan.observer.longitude, 8),
                                           number(plan.observer.heightKm, 6), number(plan.observer.minimumElevation, 3),
                                           utc(pass.rise.time));
            QStringList description{
                (partial ? tr("时段内最高点：%1") : tr("最高点：%1")).arg(iso(pass.peak.time, plan.zone)),
                tr("开始方位：%1 %2°；结束方位：%3 %4°")
                    .arg(Orbit::directionName(pass.rise.azimuth), number(pass.rise.azimuth, 1),
                         Orbit::directionName(pass.set.azimuth), number(pass.set.azimuth, 1)),
                tr("最低高度角：%1°").arg(number(plan.observer.minimumElevation, 1)),
                tr("当地时间：%1 / %2；时区：%3")
                    .arg(iso(pass.rise.time, plan.zone), iso(pass.set.time, plan.zone), zone),
                tr("轨道历元（UTC）：%1").arg(epoch),
                tr("观测地点：%1").arg(location),
                tr("海拔：%1").arg(plan.heightKnown ? number(plan.height, 1) + " m" : tr("待填写")),
                tr("光学区间（UTC）：%1").arg(optical.isEmpty() ? tr("条件欠佳") : optical.join("; ")),
                tr("数据来源：%1").arg(plan.source)};
            if (pass.startsBeforeWindow)
                description.append(tr("时段开始前已高于最低高度角"));
            if (pass.endsAfterWindow)
                description.append(tr("时段结束时仍高于最低高度角"));
            icsLine("BEGIN:VEVENT");
            icsLine("UID:" +
                    QUuid::createUuidV5(QUuid("{cb64f4af-cd0e-4d96-806d-1d73cf11da90}"), identity.toUtf8())
                        .toString(QUuid::WithoutBraces) +
                    "@astrochron");
            icsLine("DTSTAMP:" + utc(generatedAt));
            icsLine("DTSTART:" + utc(pass.rise.time));
            icsLine("DTEND:" + utc(std::max(std::floor(pass.set.time), std::floor(pass.rise.time) + 1)));
            icsLine("SUMMARY:" + icsText(title));
            icsLine("LOCATION:" + icsText(location));
            icsLine("DESCRIPTION:" + icsText(description.join('\n')));
            icsLine("END:VEVENT");
        }
    }
    if (format == PlanFormat::Ics)
        icsLine("END:VCALENDAR");
    return output;
}
