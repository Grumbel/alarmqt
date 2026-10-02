// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmDBus.h"
#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>

#ifndef ALARMQT_VERSION
#  define ALARMQT_VERSION "0.0.0-unknown"
#endif

AlarmDBus::AlarmDBus(AlarmManager* manager, MainWindow* window, QObject* parent)
    : QObject(parent)
    , m_manager(manager)
    , m_window(window)
{
}

QString AlarmDBus::formatLine(const Alarm& a) {
    const QDateTime whenSrc = a.scheduledUtc.isValid() ? a.scheduledUtc : a.triggerUtc;
    const auto local = whenSrc.toLocalTime();

    const QString st = a.statusText();

    QString remaining;
    if (a.acknowledged)
        remaining = QStringLiteral("—");
    else if (a.snoozed)
        remaining = QStringLiteral("snooze %1").arg(a.remainingString());
    else
        remaining = a.remainingString();

    QString line = QStringLiteral("%1  %2  %3  [%4]")
                       .arg(st, remaining,
                            local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")),
                            a.command);
    if (!a.label.isEmpty())
        line += QLatin1Char(' ') + a.label;
    return line;
}

QString AlarmDBus::List() {
    return ListLines().join(QLatin1Char('\n'));
}

QStringList AlarmDBus::ListLines() {
    QStringList lines;
    if (!m_manager)
        return lines;
    for (const auto& a : m_manager->alarms())
        lines.append(formatLine(a));
    return lines;
}

QString AlarmDBus::Add(const QString& expression) {
    if (!m_manager)
        return QStringLiteral("no manager");
    auto opt = AlarmManager::parse(expression);
    if (!opt)
        return QStringLiteral("parse error: %1").arg(expression);
    m_manager->add(*opt);
    if (m_window)
        m_window->raiseAndActivate();
    return {};
}

void AlarmDBus::Raise() {
    if (m_window)
        m_window->raiseAndActivate();
}

void AlarmDBus::Quit() {
    QApplication::exit(0);
}

int AlarmDBus::ClearDone() {
    if (!m_manager)
        return 0;
    return m_manager->clearDone();
}

QString AlarmDBus::Version() {
    return QStringLiteral(ALARMQT_VERSION);
}
