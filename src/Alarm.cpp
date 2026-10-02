// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Alarm.h"

#include <QJsonObject>
#include <QTimeZone>

QJsonObject Alarm::toJson() const {
    QJsonObject o;
    o["id"] = id.toString(QUuid::WithoutBraces);
    o["command"] = command;
    o["label"] = label;
    o["triggerUtc"] = triggerUtc.toString(Qt::ISODateWithMs);
    o["repeating"] = repeating;
    o["snoozeMinutes"] = snoozeMinutes;
    o["acknowledged"] = acknowledged;
    o["triggered"] = triggered;
    return o;
}

Alarm Alarm::fromJson(const QJsonObject& obj) {
    Alarm a;
    a.id = QUuid::fromString(obj["id"].toString());
    a.command = obj["command"].toString();
    a.label = obj["label"].toString();
    // Back-compat: old files only had "label" holding the expression or note
    if (a.command.isEmpty() && !a.label.isEmpty() && !obj.contains(QStringLiteral("command"))) {
        a.command = a.label;
        a.label.clear();
    }
    a.triggerUtc = QDateTime::fromString(obj["triggerUtc"].toString(), Qt::ISODateWithMs);
    a.triggerUtc.setTimeZone(QTimeZone::utc());
    a.repeating = obj["repeating"].toBool(false);
    a.snoozeMinutes = obj["snoozeMinutes"].toInt(5);
    a.acknowledged = obj["acknowledged"].toBool(false);
    a.triggered = obj["triggered"].toBool(false);
    return a;
}

QString Alarm::displayName() const {
    if (!label.isEmpty())
        return label;
    if (!command.isEmpty())
        return command;
    return QStringLiteral("(unnamed)");
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
