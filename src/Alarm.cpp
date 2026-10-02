// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Alarm.h"

#include <QTimeZone>

QJsonObject Alarm::toJson() const {
    QJsonObject o;
    o["id"] = id.toString(QUuid::WithoutBraces);
    o["command"] = command;
    o["label"] = label;
    o["triggerUtc"] = triggerUtc.toUTC().toString(Qt::ISODateWithMs);
    o["scheduledUtc"] = scheduledUtc.toUTC().toString(Qt::ISODateWithMs);
    if (recurrence.isRecurring())
        o["recurrence"] = recurrence.toJson();
    o["snoozeMinutes"] = snoozeMinutes;
    o["acknowledged"] = acknowledged;
    o["triggered"] = triggered;
    o["snoozed"] = snoozed;
    o["missed"] = missed;
    return o;
}

Alarm Alarm::fromJson(const QJsonObject& obj) {
    Alarm a;
    a.id = QUuid::fromString(obj["id"].toString());
    a.command = obj["command"].toString();
    a.label = obj["label"].toString();
    a.triggerUtc = QDateTime::fromString(obj["triggerUtc"].toString(), Qt::ISODateWithMs);
    a.triggerUtc.setTimeZone(QTimeZone::utc());
    if (obj.contains(QStringLiteral("scheduledUtc"))) {
        a.scheduledUtc = QDateTime::fromString(obj["scheduledUtc"].toString(), Qt::ISODateWithMs);
        a.scheduledUtc.setTimeZone(QTimeZone::utc());
    } else {
        a.scheduledUtc = a.triggerUtc; // migrate older saves
    }
    if (!a.scheduledUtc.isValid())
        a.scheduledUtc = a.triggerUtc;
    a.recurrence = Recurrence::fromJson(obj["recurrence"].toObject());
    a.snoozeMinutes = obj["snoozeMinutes"].toInt(5);
    a.acknowledged = obj["acknowledged"].toBool(false);
    a.triggered = obj["triggered"].toBool(false);
    a.snoozed = obj["snoozed"].toBool(false);
    a.missed = obj["missed"].toBool(false);
    return a;
}

QString Alarm::displayName() const {
    if (!label.isEmpty())
        return label;
    if (!command.isEmpty())
        return command;
    return QStringLiteral("(unnamed)");
}

QString Alarm::statusText(const QDateTime& nowUtc) const {
    if (acknowledged)
        return QStringLiteral("DONE");
    if (missed)
        return QStringLiteral("MISSED");
    if (triggered || isDue(nowUtc))
        return QStringLiteral("DUE");
    if (snoozed)
        return QStringLiteral("SNOOZED");
    return QStringLiteral("ACTIVE");
}

qint64 Alarm::remainingMs(const QDateTime& nowUtc) const {
    return nowUtc.msecsTo(triggerUtc);
}

bool Alarm::isDue(const QDateTime& nowUtc) const {
    return !acknowledged && remainingMs(nowUtc) <= 0;
}

QString Alarm::remainingString(const QDateTime& nowUtc) const {
    qint64 ms = remainingMs(nowUtc);
    if (ms <= 0) {
        if (acknowledged)
            return QStringLiteral("done");
        return QStringLiteral("NOW");
    }

    qint64 totalSec = ms / 1000;
    qint64 days = totalSec / 86400;
    qint64 hours = (totalSec % 86400) / 3600;
    qint64 mins = (totalSec % 3600) / 60;
    qint64 secs = totalSec % 60;

    QStringList parts;
    if (days > 0)
        parts << QString("%1d").arg(days);
    if (hours > 0 || days > 0)
        parts << QString("%1h").arg(hours);
    if (mins > 0 || hours > 0 || days > 0)
        parts << QString("%1m").arg(mins);
    parts << QString("%1s").arg(secs);
    return parts.join(QLatin1Char(' '));
}
