// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmDBus.h"
#include "AlarmManager.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QIcon>
#include <QDebug>
#include <iostream>

#ifndef ALARMQT_VERSION
#  define ALARMQT_VERSION "0.0.0-unknown"
#endif

static void printAlarmList(const AlarmManager& manager, bool includeDone) {
    for (const auto& a : manager.alarms()) {
        if (!includeDone && a.acknowledged)
            continue;
        const QDateTime whenSrc = a.scheduledUtc.isValid() ? a.scheduledUtc : a.triggerUtc;
        const auto local = whenSrc.toLocalTime();
        QString remaining;
        if (a.acknowledged)
            remaining = QStringLiteral("—");
        else if (a.snoozed)
            remaining = QStringLiteral("snooze %1").arg(a.remainingString());
        else
            remaining = a.remainingString();
        std::cout << a.statusText().toStdString() << "  "
                  << remaining.toStdString() << "  "
                  << local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toStdString()
                  << "  [" << a.command.toStdString() << "]"
                  << (a.label.isEmpty() ? "" : (" " + a.label.toStdString()))
                  << "\n";
    }
}

/** Call a method on the running primary; return true if the bus call was delivered. */
static bool callPrimary(const QString& method, const QVariantList& args = {},
                        QString* outString = nullptr, QStringList* outList = nullptr) {
    QDBusInterface iface(QLatin1String(AlarmDBus::serviceName()),
                         QLatin1String(AlarmDBus::objectPath()),
                         QLatin1String(AlarmDBus::interfaceName()),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        qWarning("D-Bus interface invalid: %s",
                 qPrintable(iface.lastError().message()));
        return false;
    }
    QDBusMessage reply = iface.callWithArgumentList(QDBus::Block, method, args);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning("D-Bus %s failed: %s", qPrintable(method),
                 qPrintable(reply.errorMessage()));
        return false;
    }
    if (outString && !reply.arguments().isEmpty())
        *outString = reply.arguments().at(0).toString();
    if (outList && !reply.arguments().isEmpty())
        *outList = reply.arguments().at(0).toStringList();
    return true;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("alarmqt"));
    QApplication::setApplicationDisplayName(QStringLiteral("AlarmQt"));
    QApplication::setApplicationVersion(QStringLiteral(ALARMQT_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("alarmqt"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/alarm.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Keyboard-friendly system-tray alarm / reminder"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("expression"),
                                 QStringLiteral("Alarm expression (e.g. \"in 10m stretch\")"),
                                 QStringLiteral("[expression]"));
    parser.addOption({{"q", "quit"}, QStringLiteral("Quit the running instance")});
    parser.addOption({{"l", "list"},
                      QStringLiteral("List alarms (via D-Bus if a primary is running)")});
    parser.addOption({{"r", "raise"}, QStringLiteral("Raise the existing window")});
    parser.process(app);

    const QStringList pos = parser.positionalArguments();
    const bool wantQuit = parser.isSet(QStringLiteral("quit"));
    const bool wantList = parser.isSet(QStringLiteral("list"));
    const bool wantRaise = parser.isSet(QStringLiteral("raise"));

    QDBusConnection bus = QDBusConnection::sessionBus();
    const bool busOk = bus.isConnected();
    if (!busOk)
        qWarning("No D-Bus session bus; single-instance and remote CLI disabled");

    // Own org.alarmqt.AlarmQt → we are primary. Failure → another instance holds the name.
    const bool isPrimary = !busOk
        || bus.registerService(QLatin1String(AlarmDBus::serviceName()));

    if (!isPrimary) {
        if (wantList) {
            QString text;
            if (!callPrimary(QStringLiteral("List"), {}, &text))
                return 1;
            if (!text.isEmpty()) {
                std::cout << text.toStdString();
                if (!text.endsWith(QLatin1Char('\n')))
                    std::cout << '\n';
            }
            return 0;
        }
        if (wantQuit) {
            callPrimary(QStringLiteral("Quit"));
            return 0;
        }
        if (wantRaise || pos.isEmpty()) {
            callPrimary(QStringLiteral("Raise"));
            return 0;
        }
        // Add expression on primary
        const QString expr = pos.join(QLatin1Char(' '));
        QString err;
        if (!callPrimary(QStringLiteral("Add"), {expr}, &err))
            return 1;
        if (!err.isEmpty()) {
            std::cerr << err.toStdString() << "\n";
            return 1;
        }
        std::cout << "Sent alarm to running instance: " << expr.toStdString() << "\n";
        return 0;
    }

    // Primary
    if (wantQuit) {
        // Nothing to quit (we just became primary with no prior instance)
        return 0;
    }

    AlarmManager manager;
    AlarmDBus dbusApi(&manager, nullptr);

    if (busOk) {
        if (!bus.registerObject(QLatin1String(AlarmDBus::objectPath()), &dbusApi,
                                QDBusConnection::ExportAllSlots)) {
            qWarning("Could not register D-Bus object %s: %s",
                     AlarmDBus::objectPath(),
                     qPrintable(bus.lastError().message()));
        }
    }

    // --list only: print and exit without GUI (still owned the name briefly)
    if (wantList && pos.isEmpty() && !wantRaise) {
        printAlarmList(manager, true);
        return 0;
    }

    MainWindow window(&manager);
    dbusApi.setWindow(&window);

    if (!pos.isEmpty()) {
        const QString expr = pos.join(QLatin1Char(' '));
        auto opt = AlarmManager::parse(expr);
        if (opt) {
            manager.add(*opt);
            const auto local = opt->triggerUtc.toLocalTime();
            std::cout << "Alarm added: " << opt->displayName().toStdString()
                      << "  at " << local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toStdString()
                      << "  (in " << opt->remainingString().toStdString() << ")\n";
        } else {
            std::cerr << "Could not parse alarm: " << expr.toStdString() << "\n"
                      << "Examples: in 10m stretch | in 5m, water plants | at 15:10 team call\n";
            return 1;
        }
    }

    if (wantList)
        printAlarmList(manager, true);

    window.show();
    return app.exec();
}
