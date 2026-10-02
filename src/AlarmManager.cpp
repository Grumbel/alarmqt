// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimeZone>
#include <optional>

AlarmManager::AlarmManager(QObject* parent)
    : QObject(parent)
{
    load();
    connect(&m_timer, &QTimer::timeout, this, &AlarmManager::tick);
    m_timer.start(1000); // 1 s resolution is fine
}

const Alarm* AlarmManager::alarmById(const QUuid& id) const {
    for (const auto& a : m_alarms)
        if (a.id == id)
            return &a;
    return nullptr;
}

Alarm* AlarmManager::alarmById(const QUuid& id) {
    for (auto& a : m_alarms)
        if (a.id == id)
            return &a;
    return nullptr;
}

static QDateTime parseRelative(const QString& s, const QDateTime& nowLocal) {
    // Matches: in 5m, in 2h30m, in 1d 2h, 5 minutes, etc.
    // Require at least one duration unit; anchor full string.
    static const QRegularExpression re(
        R"(\A(?:in\s+)?(?:(\d+)\s*d(?:ays?)?)?\s*(?:(\d+)\s*h(?:ours?)?)?\s*(?:(\d+)\s*m(?:in(?:utes?)?)?)?\s*(?:(\d+)\s*s(?:ec(?:onds?)?)?)?\s*\z)",
        QRegularExpression::CaseInsensitiveOption);

    auto m = re.match(s.trimmed());
    if (!m.hasMatch() || (m.captured(1).isEmpty() && m.captured(2).isEmpty()
                          && m.captured(3).isEmpty() && m.captured(4).isEmpty()))
        return {};

    qint64 secs = 0;
    if (!m.captured(1).isEmpty()) secs += m.captured(1).toLongLong() * 86400;
    if (!m.captured(2).isEmpty()) secs += m.captured(2).toLongLong() * 3600;
    if (!m.captured(3).isEmpty()) secs += m.captured(3).toLongLong() * 60;
    if (!m.captured(4).isEmpty()) secs += m.captured(4).toLongLong();
    if (secs <= 0)
        return {};

    return nowLocal.addSecs(secs);
}

static QDateTime parseAbsolute(const QString& s, const QDateTime& nowLocal) {
    QString t = s.trimmed();
    // strip leading "at "
    if (t.startsWith(QLatin1String("at "), Qt::CaseInsensitive))
        t = t.mid(3).trimmed();

    QDateTime dt;

    // Full ISO-ish: 2026-10-02 15:10 or 2026-10-02T15:10:00
    dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-dd HH:mm"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-ddTHH:mm"));

    // Time only: 15:10 or 15:10:00 → today, or tomorrow if already passed
    if (!dt.isValid()) {
        QTime time = QTime::fromString(t, QStringLiteral("HH:mm:ss"));
        if (!time.isValid())
            time = QTime::fromString(t, QStringLiteral("HH:mm"));
        if (time.isValid()) {
            dt = QDateTime(nowLocal.date(), time, nowLocal.timeZone());
            if (dt <= nowLocal)
                dt = dt.addDays(1);
        }
    }

    if (!dt.isValid())
        return {};

    // Assume local timezone if none set
    if (!dt.timeZone().isValid() || dt.timeZone() == QTimeZone::LocalTime)
        dt.setTimeZone(nowLocal.timeZone());
    return dt;
}

std::optional<Alarm> AlarmManager::parse(const QString& input, const QString& label) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    const QDateTime nowLocal = QDateTime::currentDateTime();
    QDateTime triggerLocal;

    // Prefer relative if it looks like one
    if (trimmed.contains(QRegularExpression(R"(\bin\b|\d+\s*[dhms])", QRegularExpression::CaseInsensitiveOption))) {
        triggerLocal = parseRelative(trimmed, nowLocal);
    }
    if (!triggerLocal.isValid())
        triggerLocal = parseAbsolute(trimmed, nowLocal);
    if (!triggerLocal.isValid())
        return std::nullopt;

    Alarm a;
    a.id = QUuid::createUuid();
    a.label = label.isEmpty() ? trimmed : label;
    a.triggerUtc = triggerLocal.toUTC();
    a.acknowledged = false;
    a.triggered = false;
    return a;
}

void AlarmManager::add(const Alarm& a) {
    m_alarms.append(a);
    // keep sorted by trigger time
    std::sort(m_alarms.begin(), m_alarms.end(),
              [](const Alarm& x, const Alarm& y) { return x.triggerUtc < y.triggerUtc; });
    save();
    emit alarmsChanged();
}

void AlarmManager::remove(const QUuid& id) {
    auto it = std::remove_if(m_alarms.begin(), m_alarms.end(),
                             [&](const Alarm& a) { return a.id == id; });
    if (it != m_alarms.end()) {
        m_alarms.erase(it, m_alarms.end());
        save();
        emit alarmsChanged();
    }
}

void AlarmManager::acknowledge(const QUuid& id) {
    if (auto* a = alarmById(id)) {
        a->acknowledged = true;
        a->triggered = false;
        save();
        emit alarmAcknowledged(id);
        emit alarmsChanged();
    }
}

void AlarmManager::snooze(const QUuid& id, int minutes) {
    if (auto* a = alarmById(id)) {
        const int m = minutes > 0 ? minutes : a->snoozeMinutes;
        a->triggerUtc = QDateTime::currentDateTimeUtc().addSecs(m * 60);
        a->triggered = false;
        a->acknowledged = false;
        a->label = QStringLiteral("%1 (snoozed %2m)").arg(a->label).arg(m);
        std::sort(m_alarms.begin(), m_alarms.end(),
                  [](const Alarm& x, const Alarm& y) { return x.triggerUtc < y.triggerUtc; });
        save();
        emit alarmsChanged();
    }
}

void AlarmManager::tick() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    bool changed = false;
    for (auto& a : m_alarms) {
        if (!a.acknowledged && !a.triggered && a.isDue(now)) {
            a.triggered = true;
            changed = true;
            emit alarmTriggered(a);
        }
    }
    if (changed)
        save();
    // Always emit so UI can refresh countdowns
    emit alarmsChanged();
}

std::optional<Alarm> AlarmManager::nextAlarm() const {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    std::optional<Alarm> best;
    for (const auto& a : m_alarms) {
        if (a.acknowledged)
            continue;
        if (!best || a.triggerUtc < best->triggerUtc)
            best = a;
    }
    return best;
}

QString AlarmManager::storagePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/alarms.json");
}

void AlarmManager::load() {
    QFile f(storagePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;
    m_alarms.clear();
    for (const auto& v : doc.array()) {
        if (!v.isObject())
            continue;
        Alarm a = Alarm::fromJson(v.toObject());
        if (!a.acknowledged)
            m_alarms.append(a);
    }
    std::sort(m_alarms.begin(), m_alarms.end(),
              [](const Alarm& x, const Alarm& y) { return x.triggerUtc < y.triggerUtc; });
}

void AlarmManager::save() const {
    QJsonArray arr;
    for (const auto& a : m_alarms) {
        if (a.acknowledged)
            continue; // drop finished alarms
        arr.append(a.toJson());
    }
    QFile f(storagePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}
