// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QJsonObject>

struct Alarm {
    QUuid id;
    QString command;         // original time expression, e.g. "in 5m" or "at 15:10"
    QString label;           // optional user note, e.g. "kitchen" (may be empty)
    QDateTime triggerUtc;    // absolute UTC instant
    bool repeating = false;  // currently unused (future)
    int snoozeMinutes = 5;
    bool acknowledged = false;
    bool triggered = false;

    QJsonObject toJson() const;
    static Alarm fromJson(const QJsonObject& obj);

    /** Prefer label for UI; fall back to command. */
    QString displayName() const;

    QString remainingString(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;
    qint64 remainingMs(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;
    bool isDue(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;
};
