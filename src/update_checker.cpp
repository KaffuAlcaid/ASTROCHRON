#include "update_checker.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QVersionNumber>
#include <algorithm>
#include <optional>

namespace {
struct Version {
    QVersionNumber number;
    QStringList prerelease;
};

bool numeric(const QString &value)
{
    return !value.isEmpty() && std::all_of(value.begin(), value.end(), [](QChar c) { return c >= '0' && c <= '9'; });
}

std::optional<Version> parseVersion(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral(
        "^v?((?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*))"
        "(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$"));
    const auto match = pattern.match(text);
    if (!match.hasMatch()) return std::nullopt;
    const auto core = match.captured(1);
    qsizetype suffix = 0;
    Version result{QVersionNumber::fromString(core, &suffix), match.captured(2).split('.', Qt::SkipEmptyParts)};
    if (suffix != core.size() || result.number.segmentCount() != 3) return std::nullopt;
    for (const auto &part : result.prerelease)
        if (numeric(part) && part.size() > 1 && part.startsWith('0')) return std::nullopt;
    return result;
}

int compare(const Version &left, const Version &right)
{
    const int core = QVersionNumber::compare(left.number, right.number);
    if (core) return core;
    if (left.prerelease.isEmpty() || right.prerelease.isEmpty())
        return left.prerelease.isEmpty() == right.prerelease.isEmpty() ? 0 : left.prerelease.isEmpty() ? 1 : -1;
    for (qsizetype i = 0; i < std::min(left.prerelease.size(), right.prerelease.size()); ++i) {
        const auto &a = left.prerelease[i], &b = right.prerelease[i];
        const bool an = numeric(a), bn = numeric(b);
        if (an != bn) return an ? -1 : 1;
        if (an && a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        const int order = QString::compare(a, b, Qt::CaseSensitive);
        if (order) return order;
    }
    return left.prerelease.size() == right.prerelease.size() ? 0 : left.prerelease.size() < right.prerelease.size() ? -1 : 1;
}
}

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
    const auto version = parseVersion(currentVersion());
    m_includePrereleases = m_settings.value("updates/includePrereleases", version && !version->prerelease.isEmpty()).toBool();
    clearResult();
}

QString UpdateChecker::currentVersion() const { return QCoreApplication::applicationVersion(); }

void UpdateChecker::clearResult()
{
    m_version.clear(); m_notes.clear(); m_error.clear(); m_httpStatus = 0;
    m_url = QUrl(QStringLiteral("https://github.com/KaffuAlcaid/ASTROCHRON/releases"));
}

void UpdateChecker::setIncludePrereleases(bool enabled)
{
    if (m_includePrereleases == enabled) return;
    m_includePrereleases = enabled;
    m_settings.setValue("updates/includePrereleases", enabled);
    ++m_request;
    m_state = Idle;
    clearResult();
    emit changed();
}

QString UpdateChecker::status() const
{
    switch (m_state) {
    case Idle: return {};
    case Checking: return tr("正在检查更新");
    case Current: return tr("当前已是最新版本");
    case Available: return tr("发现版本 %1").arg(m_version);
    case Empty: return tr("暂无符合条件的发布版本");
    case Failed:
        if (m_httpStatus == 403 || m_httpStatus == 429) return tr("GitHub 暂时限制了请求，请稍后再检查");
        if (m_httpStatus >= 400) return tr("更新检查失败（HTTP %1）").arg(m_httpStatus);
        return m_error.isEmpty() ? tr("更新信息格式无效") : tr("更新检查失败：%1").arg(m_error);
    }
    return {};
}

void UpdateChecker::check()
{
    if (busy()) return;
    clearResult();
    const auto current = parseVersion(currentVersion());
    if (!current) { m_state = Failed; emit changed(); return; }
    const auto serial = ++m_request;
    const bool includePreview = m_includePrereleases;
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/KaffuAlcaid/ASTROCHRON/releases?per_page=100")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ASTROCHRON/") + currentVersion());
    request.setTransferTimeout(20000);
    auto *reply = m_network.get(request);
    m_state = Checking;
    emit changed();
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial, includePreview, current] {
        reply->deleteLater();
        if (serial != m_request) return;
        m_httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            m_error = reply->errorString(); m_state = Failed; emit changed(); return;
        }
        const auto document = QJsonDocument::fromJson(reply->readAll());
        if (!document.isArray()) { m_state = Failed; emit changed(); return; }
        std::optional<Version> newest;
        QJsonObject release;
        for (const auto value : document.array()) {
            const auto item = value.toObject();
            const auto version = parseVersion(item.value("tag_name").toString());
            if (!version || item.value("draft").toBool()) continue;
            if (!includePreview && (item.value("prerelease").toBool() || !version->prerelease.isEmpty())) continue;
            if (!newest || compare(*version, *newest) > 0) { newest = version; release = item; }
        }
        if (!newest) m_state = Empty;
        else {
            m_version = release.value("tag_name").toString();
            m_url = QUrl(QStringLiteral("https://github.com/KaffuAlcaid/ASTROCHRON/releases/tag/") + QString::fromLatin1(QUrl::toPercentEncoding(m_version)));
            m_state = compare(*newest, *current) > 0 ? Available : Current;
            if (m_state == Available) m_notes = release.value("body").toString();
        }
        emit changed();
    });
}
