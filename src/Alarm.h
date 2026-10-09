// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Recurrence.h"

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QJsonObject>

struct Alarm {
    QUuid id;
    QString command;         // original time expression, e.g. "in 5m" or "at 15:10"
    QString label;           // optional user note, e.g. "stretch" (may be empty)
    QDateTime triggerUtc;    // next fire time (moves on snooze)
    QDateTime scheduledUtc;  // intended "real" alarm time (unchanged by snooze)
    Recurrence recurrence;   // repeat rule; acknowledging re-arms instead of DONE
    int snoozeMinutes = 5;
    bool acknowledged = false;
    bool triggered = false;
    bool snoozed = false;    // triggerUtc is a snooze deferral of scheduledUtc
    bool missed = false;     // was due while the app was not running
    bool disabled = false;   // paused: never fires until re-enabled

    QJsonObject toJson() const;
    static Alarm fromJson(const QJsonObject& obj);

    /** Prefer label for UI; fall back to command. */
    QString displayName() const;

    /** DONE / DISABLED / MISSED / DUE / SNOOZED / ACTIVE */
    QString statusText(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;

    QString remainingString(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;
    qint64 remainingMs(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;
    bool isDue(const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()) const;
};
