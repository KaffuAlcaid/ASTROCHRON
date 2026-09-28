#include "photometry_catalog.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>
#include <cmath>

PhotometryCatalog::PhotometryCatalog(QSqlDatabase database) : m_database(database)
{
    QFile magnitudes(":/photometry/qs.mag");
    int skipped = 0;
    if (magnitudes.open(QIODevice::ReadOnly))
        m_defaults = readMagnitudes(
            magnitudes, {0, 0, QStringLiteral("Mike McCants / QuickSat"), false, QStringLiteral("2020-09-14")},
            skipped);
    m_defaults.insert(48274, {0.87, 0, QStringLiteral("SeeSat-L / Jay Respler"), false, QStringLiteral("2022-08-03"), 0,
                              QStringLiteral("2021-035A")});
}

void PhotometryCatalog::load()
{
    QSqlQuery query(m_database);
    if (query.exec("SELECT norad, magnitude, phase, source, manual, source_date, recorded_at FROM photometry"))
        while (query.next())
            m_overrides.insert(query.value(0).toLongLong(),
                               {query.value(1).toDouble(), query.value(2).toInt(), query.value(3).toString(),
                                query.value(4).toBool(), query.value(5).toString(), query.value(6).toLongLong()});
}

bool PhotometryCatalog::hasDefault(const Orbit::Satellite *satellite) const
{
    if (!satellite)
        return false;
    const auto base = m_defaults.constFind(satellite->number);
    return base != m_defaults.cend() && !base->internationalId.isEmpty() &&
           base->internationalId == satellite->internationalId;
}

const PhotometryReference *PhotometryCatalog::entry(const Orbit::Satellite *satellite) const
{
    if (!satellite)
        return nullptr;
    const auto custom = m_overrides.constFind(satellite->number);
    if (custom != m_overrides.cend())
        return &custom.value();
    if (!hasDefault(satellite))
        return nullptr;
    return &m_defaults.constFind(satellite->number).value();
}

bool PhotometryCatalog::set(qint64 id, double magnitude, int phase, const QString &source, const QString &sourceDate)
{
    if (!std::isfinite(magnitude) || magnitude < -30 || magnitude > 30 || (phase != 0 && phase != 90))
        return false;
    const auto date = sourceDate.trimmed();
    if (!date.isEmpty() && (!QDate::fromString(date, Qt::ISODate).isValid() ||
                            QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date)) {
        m_status = [] { return tr("请填写有效的资料日期（YYYY-MM-DD）"); };
        return false;
    }
    const QString label = source.trimmed().isEmpty() ? QStringLiteral("手动填写") : source.trimmed();
    const auto recordedAt = QDateTime::currentSecsSinceEpoch();
    QSqlQuery query(m_database);
    query.prepare("INSERT OR REPLACE INTO photometry (norad, magnitude, phase, source, manual, source_date, "
                  "recorded_at) VALUES (?, ?, ?, ?, 1, ?, ?)");
    query.addBindValue(id);
    query.addBindValue(magnitude);
    query.addBindValue(phase);
    query.addBindValue(label);
    query.addBindValue(date.isEmpty() ? QStringLiteral("") : date);
    query.addBindValue(recordedAt);
    if (!query.exec()) {
        m_status = [error = query.lastError().text()] { return tr("星等参数保存失败：") + error; };
        return false;
    }
    m_overrides.insert(id, {magnitude, phase, label, true, date, recordedAt});
    m_status = [] { return tr("星等参数已保存"); };

    return true;
}

bool PhotometryCatalog::clear(const Orbit::Satellite *satellite)
{
    const auto id = satellite ? satellite->number : 0;
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM photometry WHERE norad = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        m_status = [error = query.lastError().text()] { return tr("星等参数清除失败：") + error; };
        return false;
    }
    m_overrides.remove(id);
    m_status = [hasDefault = entry(satellite) != nullptr] {
        return hasDefault ? tr("使用内置星等资料") : tr("星等参数已清除");
    };

    return true;
}

QHash<qint64, PhotometryReference> PhotometryCatalog::readMagnitudes(QIODevice &input,
                                                                     const PhotometryReference &metadata, int &skipped)
{
    QHash<qint64, PhotometryReference> entries;
    static const QRegularExpression designation(QStringLiteral("^(\\d{2})\\s+(\\d{1,3})([A-Z]{1,3})$"));
    while (!input.atEnd()) {
        const auto line = QString::fromUtf8(input.readLine());
        if (line.trimmed().isEmpty())
            continue;
        bool validId = false, validMagnitude = false;
        const auto id = line.left(5).toLongLong(&validId);
        const double magnitude = line.mid(33, 4).trimmed().toDouble(&validMagnitude);
        // QuickSat columns 34-37: magnitude at 1000 km and full phase; 20 means unknown.
        if (line.size() < 37 || !validId || id <= 0 || !validMagnitude || !std::isfinite(magnitude) ||
            magnitude == 20 || magnitude < -30 || magnitude > 30) {
            ++skipped;
            continue;
        }
        auto entry = metadata;
        entry.magnitude = magnitude;
        const auto match = designation.match(line.mid(8, 8).trimmed());
        if (match.hasMatch()) {
            const int year = match.captured(1).toInt();
            entry.internationalId = QStringLiteral("%1-%2%3")
                                        .arg(year < 57 ? 2000 + year : 1900 + year)
                                        .arg(match.captured(2).rightJustified(3, QLatin1Char('0')), match.captured(3));
        }
        entries.insert(id, entry);
    }
    return entries;
}

bool PhotometryCatalog::importMagnitudes(const QUrl &url, const QString &sourceDate)
{
    const auto fail = [this](std::function<QString()> message) {
        m_status = std::move(message);
        return false;
    };
    const auto date = sourceDate.trimmed();
    if (!date.isEmpty() && (!QDate::fromString(date, Qt::ISODate).isValid() ||
                            QDate::fromString(date, Qt::ISODate).toString(Qt::ISODate) != date))
        return fail([] { return tr("请填写有效的资料日期（YYYY-MM-DD）"); });
    const auto recordedAt = QDateTime::currentSecsSinceEpoch();
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly))
        return fail([error = file.errorString()] { return tr("星等表打开失败：") + error; });
    int skipped = 0, preserved = 0;
    auto entries = readMagnitudes(
        file, {0, 0, QStringLiteral("QuickSat · ") + QFileInfo(file).fileName(), false, date, recordedAt}, skipped);
    if (entries.isEmpty())
        return fail([] { return tr("文件中没有可用的 QuickSat 星等记录"); });
    if (!m_database.transaction())
        return fail([error = m_database.lastError().text()] { return tr("星等资料库写入失败：") + error; });
    QSqlQuery query(m_database);
    query.prepare("INSERT OR REPLACE INTO photometry (norad, magnitude, phase, source, manual, source_date, "
                  "recorded_at) VALUES (?, ?, ?, ?, 0, ?, ?)");
    for (auto it = entries.begin(); it != entries.end();) {
        if (m_overrides.value(it.key()).manual) {
            ++preserved;
            it = entries.erase(it);
            continue;
        }
        query.bindValue(0, it.key());
        query.bindValue(1, it->magnitude);
        query.bindValue(2, it->phase);
        query.bindValue(3, it->source);
        query.bindValue(4, date.isEmpty() ? QStringLiteral("") : date);
        query.bindValue(5, recordedAt);
        if (!query.exec()) {
            m_database.rollback();
            return fail([error = query.lastError().text()] { return tr("星等表导入失败：") + error; });
        }
        ++it;
    }
    if (!m_database.commit()) {
        m_database.rollback();
        return fail([error = m_database.lastError().text()] { return tr("星等表保存失败：") + error; });
    }
    for (auto it = entries.cbegin(); it != entries.cend(); ++it)
        m_overrides.insert(it.key(), it.value());
    m_status = [count = entries.size(), preserved, skipped] {
        return tr("已导入 %1 条星等记录，保留 %2 条手动参数，跳过 %3 行空缺或格式异常记录")
            .arg(count)
            .arg(preserved)
            .arg(skipped);
    };

    return true;
}
