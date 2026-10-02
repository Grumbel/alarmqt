// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AlarmManager.h"

#include <QObject>
#include <QString>
#include <QStringList>

class MainWindow;

/**
 * Session-bus API for AlarmQt.
 *
 * D-Bus well-known names must contain a '.' — we use a short id, not reverse DNS:
 *   service:   alarmqt.app
 *   path:      /alarmqt
 *   interface: alarmqt.App
 */
class AlarmDBus : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "alarmqt.App")

public:
    static constexpr const char* serviceName() { return "alarmqt.app"; }
    static constexpr const char* objectPath() { return "/alarmqt"; }
    static constexpr const char* interfaceName() { return "alarmqt.App"; }

    explicit AlarmDBus(AlarmManager* manager, MainWindow* window = nullptr,
                       QObject* parent = nullptr);

    void setWindow(MainWindow* window) { m_window = window; }

public slots:
    /** Multiline alarm list (same format as `alarmqt --list`). */
    Q_SCRIPTABLE QString List();
    /** One formatted line per alarm. */
    Q_SCRIPTABLE QStringList ListLines();
    /** Parse and add an expression. Empty string on success; error text otherwise. */
    Q_SCRIPTABLE QString Add(const QString& expression);
    Q_SCRIPTABLE void Raise();
    Q_SCRIPTABLE void Quit();
    Q_SCRIPTABLE int ClearDone();
    Q_SCRIPTABLE QString Version();

private:
    static QString formatLine(const Alarm& a);

    AlarmManager* m_manager = nullptr;
    MainWindow* m_window = nullptr;
};
