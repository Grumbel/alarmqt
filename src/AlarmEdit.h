// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Alarm.h"

#include <QString>

#include <optional>

/** User-facing lifecycle state of an alarm, as chosen in the Edit dialog. */
enum class AlarmState { Active, Disabled, Done };

AlarmState alarmState(const Alarm& a);

/**
 * What the user entered in the Edit alarm dialog.
 *
 * `expression` is the same syntax as the main input line; it is the single
 * source of the alarm time (there is no separate date/time field).
 */
struct AlarmEdit {
    QString expression;
    QString label;
    AlarmState state = AlarmState::Active;
    std::optional<NotificationStyle> style;

    /**
     * Move leading modifiers (`silent`, `disabled`, ...) and a trailing note
     * out of `expression` into `style` / `state` / `label`, leaving only the
     * time part. Returns false (and changes nothing) if it does not parse.
     */
    bool normalize();
};

struct AlarmEditOutcome {
    std::optional<Alarm> alarm; ///< nullopt on error
    QString error;              ///< user-facing reason when alarm is nullopt
    bool rescheduled = false;   ///< time was (re)computed from the expression
};

/**
 * Apply an edit to `base` (the alarm's current state).
 *
 * The time is recomputed from the expression when it differs from
 * `originalCommand` (the command shown when the dialog opened), or when a
 * DONE alarm is reopened (like Restart). Otherwise the existing schedule,
 * including a pending snooze, is kept.
 */
AlarmEditOutcome applyAlarmEdit(const Alarm& base, const QString& originalCommand,
                                AlarmEdit edit);
