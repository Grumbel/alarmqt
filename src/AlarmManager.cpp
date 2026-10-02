// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmManager.h"

#include <QDir>
#include <QHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
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

// One optional "<n> unit" group per unit, in d/h/m/s order. The lookahead
// keeps "in 5 hamburgers" from matching as "in 5h" + note "amburgers".
static const QString kDurationPattern = QStringLiteral(
    R"((?:in\s+)?)"
    R"((?:(\d+)\s*d(?:ays?)?(?![a-z]))?\s*)"
    R"((?:(\d+)\s*h(?:ours?|rs?)?(?![a-z]))?\s*)"
    R"((?:(\d+)\s*m(?:in(?:ute)?s?)?(?![a-z]))?\s*)"
    R"((?:(\d+)\s*s(?:ec(?:ond)?s?)?(?![a-z]))?)");

// Upper bound for relative alarms; also keeps the arithmetic below in range.
static constexpr qint64 kMaxRelativeSecs = 100LL * 366 * 86400;

static QDateTime parseRelative(const QString& s, const QDateTime& nowLocal) {
    // Matches: in 5m, in 2h30m, in 1d 2h, 5 minutes, etc.
    // Require at least one duration unit; anchor full string.
    static const QRegularExpression re(
        QStringLiteral(R"(\A)") + kDurationPattern + QStringLiteral(R"(\s*\z)"),
        QRegularExpression::CaseInsensitiveOption);

    auto m = re.match(s.trimmed());
    if (!m.hasMatch() || (m.captured(1).isEmpty() && m.captured(2).isEmpty()
                          && m.captured(3).isEmpty() && m.captured(4).isEmpty()))
        return {};

    static constexpr qint64 kUnitSecs[] = {86400, 3600, 60, 1};
    qint64 secs = 0;
    for (int i = 0; i < 4; ++i) {
        const QString digits = m.captured(i + 1);
        if (digits.isEmpty())
            continue;
        bool ok = false;
        const qint64 n = digits.toLongLong(&ok);
        if (!ok || n > kMaxRelativeSecs / kUnitSecs[i])
            return {};
        secs += n * kUnitSecs[i];
    }
    if (secs <= 0 || secs > kMaxRelativeSecs)
        return {};

    return nowLocal.addSecs(secs);
}


// Glued timezone on absolute times only: "15:10CEST", "15:10+02:00", "12:00Z".
// A space before a word starts a label, not a zone ("15:10 standup").
static const QString kGluedZoneSuffix = QStringLiteral(
    R"((Z|UTC|GMT|[+-]\d{2}:\d{2}|[+-]\d{4})"
    R"(|CET|CEST|EET|EEST|WET|WEST|BST|MSK)"
    R"(|EST|EDT|CST|CDT|MST|MDT|PST|PDT)"
    R"(|[A-Za-z]+/[A-Za-z0-9_+-]+))");

static QTimeZone resolveZoneToken(const QString& raw) {
    const QString t = raw.trimmed();
    if (t.isEmpty())
        return {};

    if (t.compare(QLatin1String("Z"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("UTC"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("GMT"), Qt::CaseInsensitive) == 0)
        return QTimeZone::utc();

    static const QRegularExpression offRe(
        QStringLiteral(R"(\A([+-])(\d{2}):?(\d{2})\z)"));
    if (auto m = offRe.match(t); m.hasMatch()) {
        const int sign = (m.captured(1) == QLatin1Char('-')) ? -1 : 1;
        const int h = m.captured(2).toInt();
        const int min = m.captured(3).toInt();
        if (h <= 14 && min <= 59)
            return QTimeZone(sign * (h * 3600 + min * 60));
        return {};
    }

    if (t.contains(QLatin1Char('/'))) {
        const QTimeZone z(t.toUtf8());
        return z.isValid() ? z : QTimeZone();
    }

    // Fixed offsets for common abbreviations (CEST is always +02, not "Berlin in summer").
    static const QHash<QString, int> kAbbrev = {
        {QStringLiteral("CET"), 3600},
        {QStringLiteral("CEST"), 7200},
        {QStringLiteral("EET"), 7200},
        {QStringLiteral("EEST"), 10800},
        {QStringLiteral("WET"), 0},
        {QStringLiteral("WEST"), 3600},
        {QStringLiteral("BST"), 3600},
        {QStringLiteral("MSK"), 10800},
        {QStringLiteral("EST"), -5 * 3600},
        {QStringLiteral("EDT"), -4 * 3600},
        {QStringLiteral("CST"), -6 * 3600},
        {QStringLiteral("CDT"), -5 * 3600},
        {QStringLiteral("MST"), -7 * 3600},
        {QStringLiteral("MDT"), -6 * 3600},
        {QStringLiteral("PST"), -8 * 3600},
        {QStringLiteral("PDT"), -7 * 3600},
    };
    const auto it = kAbbrev.constFind(t.toUpper());
    if (it != kAbbrev.cend())
        return QTimeZone(*it);
    return {};
}

/** If text ends with a glued zone token, peel it into *zone and leave the time text. */
static bool peelGluedZone(QString* text, QTimeZone* zone) {
    // The time must end in a digit or am/pm, so "15:10CEST" peels "CEST"
    // rather than "15:10C" + "EST".
    static const QRegularExpression trailing(
        QStringLiteral(R"(\A(.*?(?:\d|[ap]\.?m\.?)))") + kGluedZoneSuffix + QStringLiteral(R"(\z)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = trailing.match(*text);
    if (!m.hasMatch())
        return false;
    const QTimeZone z = resolveZoneToken(m.captured(2));
    if (!z.isValid())
        return false;
    *text = m.captured(1);
    *zone = z;
    return true;
}

static QDateTime parseAbsolute(const QString& s, const QDateTime& nowLocal) {
    QString t = s.trimmed();
    // strip leading "at "
    if (t.startsWith(QLatin1String("at "), Qt::CaseInsensitive))
        t = t.mid(3).trimmed();

    QTimeZone zone = nowLocal.timeZone();
    bool hadGluedZone = peelGluedZone(&t, &zone);
    if (hadGluedZone && !zone.isValid())
        return {};

    QDateTime dt;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    auto finalizeTimeOnly = [&](const QTime& time) -> QDateTime {
        if (!time.isValid())
            return {};
        // Calendar date in the target zone (today there), not necessarily local date.
        const QDate zoneToday = QDateTime(nowUtc).toTimeZone(zone).date();
        QDateTime candidate(zoneToday, time, zone);
        if (candidate.toUTC() <= nowUtc)
            candidate = candidate.addDays(1);
        return candidate;
    };

    // Full ISO-ish: 2026-10-02 15:10 or 2026-10-02T15:10:00
    dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-dd HH:mm"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(t, QStringLiteral("yyyy-MM-ddTHH:mm"));
    if (dt.isValid()) {
        // fromString yields local/no zone; pin the intended zone.
        dt.setTimeZone(zone);
        return dt;
    }

    // Time only (24h): 15:10 or 15:10:00
    {
        QTime time = QTime::fromString(t, QStringLiteral("HH:mm:ss"));
        if (!time.isValid())
            time = QTime::fromString(t, QStringLiteral("HH:mm"));
        dt = finalizeTimeOnly(time);
        if (dt.isValid())
            return dt;
    }

    // American 12-hour: 6:00am, 6:00 pm, 6am, 6 PM, 12:00am/pm
    {
        QString norm = t;
        norm.replace(QLatin1Char('.'), QString());
        static const QRegularExpression amPmReFlat(
            R"(\A(\d{1,2})(?::(\d{2})(?::(\d{2}))?)?\s*([ap])m\z)",
            QRegularExpression::CaseInsensitiveOption);
        if (auto m = amPmReFlat.match(norm.trimmed()); m.hasMatch()) {
            int hour = m.captured(1).toInt();
            const int minute = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
            const int second = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
            const bool pm = m.captured(4).compare(QLatin1String("p"), Qt::CaseInsensitive) == 0;
            if (hour >= 1 && hour <= 12 && minute <= 59 && second <= 59) {
                if (pm && hour < 12)
                    hour += 12;
                else if (!pm && hour == 12)
                    hour = 0;
                dt = finalizeTimeOnly(QTime(hour, minute, second));
                if (dt.isValid())
                    return dt;
            }
        }
    }

    return {};
}

// Split "time expression" + optional human note.
// Supported note forms (first match wins):
//   in 5s, kitchen
//   in 5s (kitchen)  /  in 5s "kitchen"  /  in 5s 'kitchen'
//   in 5s kitchen          (trailing words after a relative duration)
//   at 15:10 meeting       (trailing words after an absolute time)
static void splitTimeAndNote(const QString& input, QString* timePart, QString* note) {
    const QString trimmed = input.trimmed();
    *timePart = trimmed;
    *note = {};

    // 1) Comma: "in 5s, kitchen"
    if (const int comma = trimmed.indexOf(QLatin1Char(',')); comma > 0) {
        *timePart = trimmed.left(comma).trimmed();
        *note = trimmed.mid(comma + 1).trimmed();
        return;
    }

    // 2) Parentheses / quotes at end
    static const QRegularExpression noteRe(
        R"(\A(.+?)\s*(?:\(([^)]+)\)|\"([^\"]+)\"|'([^']+)')\s*\z)");
    if (auto nm = noteRe.match(trimmed); nm.hasMatch()) {
        *timePart = nm.captured(1).trimmed();
        *note = nm.captured(2);
        if (note->isEmpty())
            *note = nm.captured(3);
        if (note->isEmpty())
            *note = nm.captured(4);
        *note = note->trimmed();
        return;
    }

    // 3) Relative duration prefix + trailing words: "in 5s kitchen"
    static const QRegularExpression relPrefix(
        QStringLiteral(R"(\A()") + kDurationPattern + QStringLiteral(")"),
        QRegularExpression::CaseInsensitiveOption);
    if (auto rm = relPrefix.match(trimmed); rm.hasMatch()) {
        const QString prefix = rm.captured(1).trimmed();
        // Must include at least one digit+unit (reject empty / "in" alone)
        static const QRegularExpression hasUnit(
            R"(\d+\s*[dhms])", QRegularExpression::CaseInsensitiveOption);
        if (hasUnit.match(prefix).hasMatch()) {
            const QString rest = trimmed.mid(rm.capturedLength(0)).trimmed();
            if (!rest.isEmpty()) {
                *timePart = prefix;
                *note = rest;
                return;
            }
        }
    }

    // 4) Absolute time prefix (+ optional glued zone) + trailing words:
    // "at 15:10 meeting" / "at 15:10CEST standup"
    static const QRegularExpression absPrefix(
        // The zone must follow the time directly, so "15:10 CEST" stays a label;
        // only am/pm may be separated by a space.
        QStringLiteral(R"(\A((?:at\s+)?(?:\d{4}-\d{2}-\d{2}[ T]\d{1,2}:\d{2}(?::\d{2})?|\d{1,2}:\d{2}(?::\d{2})?(?:\s*[ap]\.?m\.?)?|\d{1,2}\s*[ap]\.?m\.?))")
        + kGluedZoneSuffix + QStringLiteral(R"(?))"),
        QRegularExpression::CaseInsensitiveOption);
    if (auto am = absPrefix.match(trimmed); am.hasMatch()) {
        const QString prefix = am.captured(1).trimmed();
        const QString rest = trimmed.mid(am.capturedLength(0)).trimmed();
        if (!rest.isEmpty()) {
            *timePart = prefix;
            *note = rest;
            return;
        }
    }
}

std::optional<Alarm> AlarmManager::parse(const QString& input, const QString& label) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    QString timePart;
    QString note;
    splitTimeAndNote(trimmed, &timePart, &note);

    const QDateTime nowLocal = QDateTime::currentDateTime();
    QDateTime triggerLocal;

    // Prefer relative if it looks like one
    if (timePart.contains(QRegularExpression(R"(\bin\b|\d+\s*[dhms])", QRegularExpression::CaseInsensitiveOption))) {
        triggerLocal = parseRelative(timePart, nowLocal);
    }
    if (!triggerLocal.isValid())
        triggerLocal = parseAbsolute(timePart, nowLocal);
    if (!triggerLocal.isValid())
        return std::nullopt;

    Alarm a;
    a.id = QUuid::createUuid();
    a.command = timePart;
    if (!label.isEmpty())
        a.label = label;
    else
        a.label = note; // may be empty
    a.triggerUtc = triggerLocal.toUTC();
    a.scheduledUtc = a.triggerUtc;
    a.acknowledged = false;
    a.triggered = false;
    a.snoozed = false;
    a.missed = false;
    return a;
}

void AlarmManager::sortAlarms() {
    std::stable_sort(m_alarms.begin(), m_alarms.end(),
                     [](const Alarm& x, const Alarm& y) { return x.triggerUtc < y.triggerUtc; });
}

void AlarmManager::add(const Alarm& a) {
    m_alarms.append(a);
    sortAlarms();
    save();
    emit alarmsChanged();
}

void AlarmManager::update(const Alarm& a) {
    if (auto* existing = alarmById(a.id)) {
        *existing = a;
        sortAlarms();
        save();
        emit alarmsChanged();
    }
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
        a->snoozed = false;
        a->missed = false;
        save();
        emit alarmAcknowledged(id);
        emit alarmsChanged();
    }
}

void AlarmManager::snooze(const QUuid& id, int minutes) {
    if (auto* a = alarmById(id)) {
        const int m = minutes > 0 ? minutes : a->snoozeMinutes;
        // Keep scheduledUtc as the original "real" alarm time; only move the
        // next fire (triggerUtc). Mark as snoozed so the UI can show both.
        if (!a->scheduledUtc.isValid())
            a->scheduledUtc = a->triggerUtc;
        a->triggerUtc = QDateTime::currentDateTimeUtc().addSecs(m * 60);
        a->triggered = false;
        a->acknowledged = false;
        a->snoozed = true;
        a->missed = false;
        sortAlarms();
        save();
        emit alarmsChanged();
    }
}

int AlarmManager::clearDone() {
    const auto oldSize = m_alarms.size();
    m_alarms.erase(std::remove_if(m_alarms.begin(), m_alarms.end(),
                                  [](const Alarm& a) { return a.acknowledged; }),
                   m_alarms.end());
    const int removed = static_cast<int>(oldSize - m_alarms.size());
    if (removed > 0) {
        save();
        emit alarmsChanged();
    }
    return removed;
}

bool AlarmManager::restart(const QUuid& id) {
    Alarm* a = alarmById(id);
    if (!a)
        return false;

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    QString expr = a->command;
    if (expr.isEmpty())
        expr = a->label;

    if (!expr.isEmpty()) {
        auto opt = parse(expr, a->label);
        if (opt) {
            a->command = opt->command.isEmpty() ? expr : opt->command;
            a->triggerUtc = opt->triggerUtc;
            // Never schedule in the past: roll full datetimes forward day by day
            while (a->triggerUtc <= nowUtc)
                a->triggerUtc = a->triggerUtc.addDays(1);
            a->scheduledUtc = a->triggerUtc;
            a->acknowledged = false;
            a->triggered = false;
            a->snoozed = false;
            a->missed = false;
            sortAlarms();
            save();
            emit alarmsChanged();
            return true;
        }
    }

    // Fallback: fire again in snoozeMinutes
    a->triggerUtc = nowUtc.addSecs(a->snoozeMinutes * 60);
    a->scheduledUtc = a->triggerUtc;
    a->acknowledged = false;
    a->triggered = false;
    a->snoozed = false;
    a->missed = false;
    sortAlarms();
    save();
    emit alarmsChanged();
    return true;
}

void AlarmManager::tick() {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    // Collect first: receivers may modify the alarm list (ack, snooze, ...).
    QVector<Alarm> fired;
    for (auto& a : m_alarms) {
        if (!a.acknowledged && !a.triggered && a.isDue(now)) {
            a.triggered = true;
            fired.append(a);
        }
    }
    if (!fired.isEmpty())
        save();
    for (const Alarm& a : std::as_const(fired))
        emit alarmTriggered(a);
    // Always emit so UI can refresh countdowns
    emit alarmsChanged();
}

std::optional<Alarm> AlarmManager::nextAlarm() const {
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
    // XDG Base Directory Spec: state goes under $XDG_STATE_HOME (default ~/.local/state).
    QString stateHome = QString::fromLocal8Bit(qgetenv("XDG_STATE_HOME"));
    if (stateHome.isEmpty())
        stateHome = QDir::homePath() + QStringLiteral("/.local/state");
    const QString dir = stateHome + QStringLiteral("/alarmqt");
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
        if (!a.triggerUtc.isValid()) {
            qWarning("Skipping stored alarm with invalid trigger time: %s",
                     qPrintable(a.displayName()));
            continue;
        }
        if (a.id.isNull())
            a.id = QUuid::createUuid();
        m_alarms.append(a);
    }
    sortAlarms();

    // After restart, re-arm due alarms so tick() will notify again.
    // Mark them missed when the trigger time passed while the app was down.
    const QDateTime now = QDateTime::currentDateTimeUtc();
    bool changed = false;
    for (auto& a : m_alarms) {
        if (!a.acknowledged && a.isDue(now)) {
            a.triggered = false;
            a.missed = true;
            changed = true;
        }
    }
    if (changed)
        save();
}

void AlarmManager::save() const {
    QJsonArray arr;
    for (const auto& a : m_alarms)
        arr.append(a.toJson());
    // Write atomically so a crash or full disk cannot truncate the alarm list.
    QSaveFile f(storagePath());
    if (!f.open(QIODevice::WriteOnly)
        || f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented)) < 0
        || !f.commit())
        qWarning("Could not save alarms to %s: %s",
                 qPrintable(f.fileName()), qPrintable(f.errorString()));
}
