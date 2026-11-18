// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmEdit.h"

#include "AlarmManager.h"

#include <QCoreApplication>

AlarmState alarmState(const Alarm& a) {
    if (a.acknowledged)
        return AlarmState::Done;
    if (a.disabled)
        return AlarmState::Disabled;
    return AlarmState::Active;
}

bool AlarmEdit::normalize() {
    const auto opt = AlarmManager::parse(expression);
    if (!opt)
        return false;
    if (opt->notificationStyle)
        style = opt->notificationStyle;
    if (opt->disabled)
        state = AlarmState::Disabled;
    if (!opt->label.isEmpty())
        label = opt->label;
    if (!opt->command.isEmpty())
        expression = opt->command;
    return true;
}

static QString tr(const char* text) {
    return QCoreApplication::translate("AlarmEdit", text);
}

AlarmEditOutcome applyAlarmEdit(const Alarm& base, const QString& originalCommand,
                                AlarmEdit edit) {
    AlarmEditOutcome out;
    Alarm a = base;

    edit.expression = edit.expression.trimmed();
    // Compare before normalizing: an untouched expression must never
    // reschedule, even if re-parsing would spell it differently.
    const bool changed = edit.expression != originalCommand.trimmed();
    if (changed) {
        if (edit.expression.isEmpty()) {
            out.error = tr("Enter a time, e.g. “in 5m” or “at 15:10”.");
            return out;
        }
        if (!edit.normalize()) {
            out.error = tr("Time not understood.");
            return out;
        }
    }
    const bool reopening = alarmState(base) == AlarmState::Done
        && edit.state != AlarmState::Done;

    if (changed || reopening) {
        const auto opt = AlarmManager::parse(edit.expression);
        if (!opt) {
            out.error = tr("Time not understood.");
            return out;
        }
        a.command = opt->command.isEmpty() ? edit.expression : opt->command;
        a.recurrence = opt->recurrence;
        a.triggerUtc = opt->triggerUtc;
        a.scheduledUtc = opt->triggerUtc;
        a.triggered = false;
        a.snoozed = false;
        a.missed = false;
        out.rescheduled = true;
    }

    a.label = edit.label.trimmed();
    a.notificationStyle = edit.style;

    if (edit.state != alarmState(base)) {
        switch (edit.state) {
        case AlarmState::Done:
            if (a.recurrence.isRecurring()) {
                out.error = tr("A repeating alarm cannot be marked done.");
                return out;
            }
            a.acknowledged = true;
            a.disabled = false;
            a.triggered = false;
            break;
        case AlarmState::Disabled:
            a.acknowledged = false;
            a.disabled = true;
            a.triggered = false; // stop an in-progress notification
            break;
        case AlarmState::Active:
            a.acknowledged = false;
            a.disabled = false;
            a.missed = false; // past-due fires on the next tick, like Enable
            break;
        }
    } else if (edit.state == AlarmState::Done && a.recurrence.isRecurring()) {
        out.error = tr("A repeating alarm cannot be marked done.");
        return out;
    }

    out.alarm = a;
    return out;
}
