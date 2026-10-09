// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QTime>
#include <QTimeZone>

/**
 * Repeat rule of an alarm.
 *
 * Interval: fires again intervalSecs after acknowledgement ("every 5m").
 * Weekly:   local wall-clock on days in `weekdays`; optional weekStride
 *           ("every monday at 18:00", "every 2 weeks on monday at 9:00").
 * Monthly:  day-of-month ("every month on the 6th at 9:00") or Nth weekday
 *           ("every 2nd tuesday at 18:00"); short months / missing Nth skipped.
 * Yearly:   month + day-of-month ("every year on 10-06 at 9:00").
 */
struct Recurrence {
    enum class Kind { None, Interval, Weekly, Monthly, Yearly };

    static constexpr quint8 kMonday = 1 << 0;
    static constexpr quint8 kWeekdays = 0x1f;
    static constexpr quint8 kWeekend = 0x60;
    static constexpr quint8 kEveryDay = 0x7f;

    Kind kind = Kind::None;
    qint64 intervalSecs = 0; // Interval
    QTime time;              // Weekly / Monthly / Yearly: local wall-clock
    quint8 weekdays = 0;     // Weekly; Monthly Nth-weekday: single day bit
    int dayOfMonth = 0;      // Monthly (calendar day) / Yearly: 1–31
    int month = 0;           // Yearly: 1–12
    int weekOrdinal = 0;     // Monthly Nth weekday: 1–5, or -1 = last
    int weekStride = 1;      // Weekly: 1 = every week, 2 = every 2 weeks, …

    bool isRecurring() const { return kind != Kind::None; }

    QDateTime nextAfter(const QDateTime& afterUtc,
                        const QTimeZone& zone = QTimeZone::systemTimeZone()) const;

    QString describe() const;

    QJsonObject toJson() const;
    static Recurrence fromJson(const QJsonObject& obj);

    bool operator==(const Recurrence&) const = default;
};
