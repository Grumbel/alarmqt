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
 * Interval: fires again intervalSecs after it was acknowledged ("every 5m").
 * Weekly:   fires at a local wall-clock time on the days in `weekdays`
 *           ("every monday at 18:00", "daily at 7:30").
 */
struct Recurrence {
    enum class Kind { None, Interval, Weekly };

    // Bit (dayOfWeek - 1), i.e. Monday = bit 0 … Sunday = bit 6.
    static constexpr quint8 kMonday = 1 << 0;
    static constexpr quint8 kWeekdays = 0x1f; // Mon..Fri
    static constexpr quint8 kWeekend = 0x60;  // Sat, Sun
    static constexpr quint8 kEveryDay = 0x7f;

    Kind kind = Kind::None;
    qint64 intervalSecs = 0; // Interval
    QTime time;              // Weekly: local wall-clock time
    quint8 weekdays = 0;     // Weekly: day mask

    bool isRecurring() const { return kind != Kind::None; }

    /**
     * Next occurrence strictly after `afterUtc`.
     * Weekly rules are evaluated in `zone`, so 18:00 stays 18:00 across DST.
     * Returns an invalid QDateTime for Kind::None or a malformed rule.
     */
    QDateTime nextAfter(const QDateTime& afterUtc,
                        const QTimeZone& zone = QTimeZone::systemTimeZone()) const;

    /** Short human description, e.g. "every 5m", "Mon, Thu 18:00", "daily 07:30". */
    QString describe() const;

    QJsonObject toJson() const;
    static Recurrence fromJson(const QJsonObject& obj);

    bool operator==(const Recurrence&) const = default;
};
