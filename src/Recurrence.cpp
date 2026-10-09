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

static QString ordinalWord(int n) {
    if (n < 0)
        return QStringLiteral("last");
    switch (n) {
    case 1:
        return QStringLiteral("1st");
    case 2:
        return QStringLiteral("2nd");
    case 3:
        return QStringLiteral("3rd");
    default:
        return QStringLiteral("%1th").arg(n);
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
        const int stride = weekStride > 0 ? weekStride : 1;
        const QDate start = afterUtc.toTimeZone(zone).date();
        // Search enough days for stride weeks of candidates.
        for (int i = 0; i <= 7 * stride + 1; ++i) {
            const QDate date = start.addDays(i);
            if (!(weekdays & (1 << (date.dayOfWeek() - 1))))
                continue;
            const QDateTime candidate(date, time, zone);
            if (!candidate.isValid() || candidate.toUTC() <= afterUtc)
                continue;
            if (stride <= 1)
                return candidate.toUTC();
            // Align to weekStride using ISO week distance from a fixed epoch Monday.
            const QDate epoch(1970, 1, 5); // Monday
            const qint64 weeks = epoch.daysTo(date) / 7;
            if (weeks % stride == 0)
                return candidate.toUTC();
        }
        // stride>1: keep searching further
        for (int i = 7 * stride + 2; i <= 7 * stride * 4; ++i) {
            const QDate date = start.addDays(i);
            if (!(weekdays & (1 << (date.dayOfWeek() - 1))))
                continue;
            const QDateTime candidate(date, time, zone);
            if (!candidate.isValid() || candidate.toUTC() <= afterUtc)
                continue;
            const QDate epoch(1970, 1, 5);
            const qint64 weeks = epoch.daysTo(date) / 7;
            if (weeks % stride == 0)
                return candidate.toUTC();
        }
        return {};
    }

    case Kind::Monthly: {
        if (!time.isValid())
            return {};
        // Nth weekday of month (weekOrdinal != 0)
        if (weekOrdinal != 0 && weekdays != 0) {
            int weekday = -1;
            for (int d = 0; d < 7; ++d) {
                if (weekdays & (1 << d)) {
                    weekday = d + 1; // Qt: Mon=1
                    break;
                }
            }
            if (weekday < 1)
                return {};
            const QDate start = afterUtc.toTimeZone(zone).date();
            int year = start.year();
            int month = start.month();
            for (int i = 0; i < 48; ++i) {
                QDate candidateDate;
                if (weekOrdinal > 0) {
                    // Nth weekday: first day of month, advance to weekday, +7*(n-1)
                    QDate d(year, month, 1);
                    int delta = (weekday - d.dayOfWeek() + 7) % 7;
                    d = d.addDays(delta + 7 * (weekOrdinal - 1));
                    if (d.month() == month)
                        candidateDate = d;
                } else {
                    // Last weekday: last day of month, walk back
                    QDate d(year, month, QDate(year, month, 1).daysInMonth());
                    while (d.dayOfWeek() != weekday)
                        d = d.addDays(-1);
                    candidateDate = d;
                }
                if (candidateDate.isValid()) {
                    const QDateTime candidate(candidateDate, time, zone);
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

        // Calendar day-of-month
        if (dayOfMonth < 1 || dayOfMonth > 31)
            return {};
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

    case Kind::Yearly: {
        if (!time.isValid() || month < 1 || month > 12 || dayOfMonth < 1 || dayOfMonth > 31)
            return {};
        const QDate start = afterUtc.toTimeZone(zone).date();
        int year = start.year();
        for (int i = 0; i < 20; ++i) {
            if (QDate::isValid(year, month, dayOfMonth)) {
                const QDateTime candidate(QDate(year, month, dayOfMonth), time, zone);
                if (candidate.isValid() && candidate.toUTC() > afterUtc)
                    return candidate.toUTC();
            }
            ++year;
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
        if (weekdays == kEveryDay)
            days = QStringLiteral("daily");
        else if (weekdays == kWeekdays)
            days = QStringLiteral("weekdays");
        else if (weekdays == kWeekend)
            days = QStringLiteral("weekends");
        else {
            QStringList names;
            const QLocale c = QLocale::c();
            for (int d = 1; d <= 7; ++d)
                if (weekdays & (1 << (d - 1)))
                    names << c.dayName(d, QLocale::ShortFormat);
            days = names.join(QStringLiteral(", "));
        }
        if (weekStride > 1)
            return QStringLiteral("every %1 weeks %2 %3").arg(weekStride).arg(days, t);
        return days + QLatin1Char(' ') + t;
    }
    case Kind::Monthly: {
        const QString t = time.toString(time.second() ? QStringLiteral("HH:mm:ss")
                                                      : QStringLiteral("HH:mm"));
        if (weekOrdinal != 0 && weekdays != 0) {
            QString dayName;
            const QLocale c = QLocale::c();
            for (int d = 1; d <= 7; ++d)
                if (weekdays & (1 << (d - 1))) {
                    dayName = c.dayName(d, QLocale::ShortFormat);
                    break;
                }
            return QStringLiteral("monthly %1 %2 %3")
                .arg(ordinalWord(weekOrdinal), dayName, t);
        }
        return QStringLiteral("monthly %1 %2").arg(ordinalDay(dayOfMonth), t);
    }
    case Kind::Yearly: {
        const QString t = time.toString(time.second() ? QStringLiteral("HH:mm:ss")
                                                      : QStringLiteral("HH:mm"));
        return QStringLiteral("yearly %1-%2 %3")
            .arg(month, 2, 10, QLatin1Char('0'))
            .arg(dayOfMonth, 2, 10, QLatin1Char('0'))
            .arg(t);
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
        if (weekStride > 1)
            o["weekStride"] = weekStride;
        break;
    case Kind::Monthly:
        o["kind"] = QStringLiteral("monthly");
        o["time"] = time.toString(QStringLiteral("HH:mm:ss"));
        if (weekOrdinal != 0) {
            o["weekOrdinal"] = weekOrdinal;
            o["weekdays"] = weekdays;
        } else {
            o["dayOfMonth"] = dayOfMonth;
        }
        break;
    case Kind::Yearly:
        o["kind"] = QStringLiteral("yearly");
        o["time"] = time.toString(QStringLiteral("HH:mm:ss"));
        o["month"] = month;
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
        r.weekStride = obj.contains(QStringLiteral("weekStride")) ? obj["weekStride"].toInt() : 1;
        if (r.weekStride < 1)
            r.weekStride = 1;
        if (!r.time.isValid() || r.weekdays == 0)
            r = {};
    } else if (kind == QLatin1String("monthly")) {
        r.kind = Kind::Monthly;
        r.time = QTime::fromString(obj["time"].toString(), QStringLiteral("HH:mm:ss"));
        if (obj.contains(QStringLiteral("weekOrdinal"))) {
            r.weekOrdinal = obj["weekOrdinal"].toInt();
            r.weekdays = static_cast<quint8>(obj["weekdays"].toInt() & kEveryDay);
            if (!r.time.isValid() || r.weekOrdinal == 0 || r.weekdays == 0)
                r = {};
        } else {
            r.dayOfMonth = obj["dayOfMonth"].toInt();
            if (!r.time.isValid() || r.dayOfMonth < 1 || r.dayOfMonth > 31)
                r = {};
        }
    } else if (kind == QLatin1String("yearly")) {
        r.kind = Kind::Yearly;
        r.time = QTime::fromString(obj["time"].toString(), QStringLiteral("HH:mm:ss"));
        r.month = obj["month"].toInt();
        r.dayOfMonth = obj["dayOfMonth"].toInt();
        if (!r.time.isValid() || r.month < 1 || r.month > 12 || r.dayOfMonth < 1
            || r.dayOfMonth > 31)
            r = {};
    }
    return r;
}
