// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Recurrence.h"

#include <QLocale>
#include <QStringList>

static QString ordinalDay(int day) {
    if (day % 100 >= 11 && day % 100 <= 13)
        return QStringLiteral("%1th").arg(day);
    switch (day % 10) {
    case 1:
        return QStringLiteral("%1st").arg(day);
    case 2:
        return QStringLiteral("%1nd").arg(day);
    case 3:
        return QStringLiteral("%1rd").arg(day);
    default:
        return QStringLiteral("%1th").arg(day);
    }
}

QDateTime Recurrence::nextAfter(const QDateTime& afterUtc, const QTimeZone& zone) const {
    switch (kind) {
    case Kind::None:
        return {};

    case Kind::Interval:
        if (intervalSecs <= 0)
            return {};
        return afterUtc.toUTC().addSecs(intervalSecs);

    case Kind::Weekly: {
        if (!time.isValid() || (weekdays & kEveryDay) == 0)
            return {};
        // Build each candidate from the calendar date and wall-clock time in
        // `zone` rather than adding 24h steps, so DST shifts keep the local
        // time. A time inside a spring-forward gap is moved past the gap by
        // QDateTime. Eight days always reach the next matching weekday.
        const QDate start = afterUtc.toTimeZone(zone).date();
        for (int i = 0; i <= 8; ++i) {
            const QDate date = start.addDays(i);
            if (!(weekdays & (1 << (date.dayOfWeek() - 1))))
                continue;
            const QDateTime candidate(date, time, zone);
            if (candidate.isValid() && candidate.toUTC() > afterUtc)
                return candidate.toUTC();
        }
        return {};
    }

    case Kind::Monthly: {
        if (!time.isValid() || dayOfMonth < 1 || dayOfMonth > 31)
            return {};
        // Local wall-clock: pick dayOfMonth in zone; skip months that lack it
        // (31 in February, etc.). Search up to 48 months ahead.
        const QDate start = afterUtc.toTimeZone(zone).date();
        int year = start.year();
        int month = start.month();
        for (int i = 0; i < 48; ++i) {
            const QDate first(year, month, 1);
            if (!first.isValid())
                return {};
            if (dayOfMonth <= first.daysInMonth()) {
                const QDateTime candidate(QDate(year, month, dayOfMonth), time, zone);
                if (candidate.isValid() && candidate.toUTC() > afterUtc)
                    return candidate.toUTC();
            }
            ++month;
            if (month > 12) {
                month = 1;
                ++year;
            }
        }
        return {};
    }
    }
    return {};
}

static QString formatInterval(qint64 secs) {
    QString out;
    const qint64 d = secs / 86400;
    const qint64 h = (secs % 86400) / 3600;
    const qint64 m = (secs % 3600) / 60;
    const qint64 s = secs % 60;
    if (d)
        out += QStringLiteral("%1d").arg(d);
    if (h)
        out += QStringLiteral("%1h").arg(h);
    if (m)
        out += QStringLiteral("%1m").arg(m);
    if (s)
        out += QStringLiteral("%1s").arg(s);
    return out;
}

QString Recurrence::describe() const {
    switch (kind) {
    case Kind::None:
        return {};
    case Kind::Interval:
        return QStringLiteral("every %1").arg(formatInterval(intervalSecs));
    case Kind::Weekly: {
        const QString t = time.toString(time.second() ? QStringLiteral("HH:mm:ss")
                                                      : QStringLiteral("HH:mm"));
        QString days;
        if (weekdays == kEveryDay) {
            days = QStringLiteral("daily");
        } else if (weekdays == kWeekdays) {
            days = QStringLiteral("weekdays");
        } else if (weekdays == kWeekend) {
            days = QStringLiteral("weekends");
        } else {
            QStringList names;
            const QLocale c = QLocale::c();
            for (int d = 1; d <= 7; ++d)
                if (weekdays & (1 << (d - 1)))
                    names << c.dayName(d, QLocale::ShortFormat);
            days = names.join(QStringLiteral(", "));
        }
        return days + QLatin1Char(' ') + t;
    }
    case Kind::Monthly: {
        const QString t = time.toString(time.second() ? QStringLiteral("HH:mm:ss")
                                                      : QStringLiteral("HH:mm"));
        return QStringLiteral("monthly %1 %2").arg(ordinalDay(dayOfMonth), t);
    }
    }
    return {};
}

QJsonObject Recurrence::toJson() const {
    QJsonObject o;
    switch (kind) {
    case Kind::None:
        break;
    case Kind::Interval:
        o["kind"] = QStringLiteral("interval");
        o["intervalSecs"] = intervalSecs;
        break;
    case Kind::Weekly:
        o["kind"] = QStringLiteral("weekly");
        o["time"] = time.toString(QStringLiteral("HH:mm:ss"));
        o["weekdays"] = weekdays;
        break;
    case Kind::Monthly:
        o["kind"] = QStringLiteral("monthly");
        o["time"] = time.toString(QStringLiteral("HH:mm:ss"));
        o["dayOfMonth"] = dayOfMonth;
        break;
    }
    return o;
}

Recurrence Recurrence::fromJson(const QJsonObject& obj) {
    Recurrence r;
    const QString kind = obj["kind"].toString();
    if (kind == QLatin1String("interval")) {
        r.kind = Kind::Interval;
        r.intervalSecs = obj["intervalSecs"].toInteger();
        if (r.intervalSecs <= 0)
            r = {};
    } else if (kind == QLatin1String("weekly")) {
        r.kind = Kind::Weekly;
        r.time = QTime::fromString(obj["time"].toString(), QStringLiteral("HH:mm:ss"));
        r.weekdays = static_cast<quint8>(obj["weekdays"].toInt() & kEveryDay);
        if (!r.time.isValid() || r.weekdays == 0)
            r = {};
    } else if (kind == QLatin1String("monthly")) {
        r.kind = Kind::Monthly;
        r.time = QTime::fromString(obj["time"].toString(), QStringLiteral("HH:mm:ss"));
        r.dayOfMonth = obj["dayOfMonth"].toInt();
        if (!r.time.isValid() || r.dayOfMonth < 1 || r.dayOfMonth > 31)
            r = {};
    }
    return r;
}
