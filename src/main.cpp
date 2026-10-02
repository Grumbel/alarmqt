// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmManager.h"
#include "MainWindow.h"
#include "SingleInstance.h"

#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QDebug>
#include <iostream>

#ifndef ALARMQT_VERSION
#  define ALARMQT_VERSION "0.0.0-unknown"
#endif

static const QString kAppKey = QStringLiteral("alarmqt-single-instance-v1");

static QString statusLabel(const Alarm& a) {
    if (a.acknowledged)
        return QStringLiteral("DONE");
    if (a.missed)
        return QStringLiteral("MISSED");
    if (a.triggered)
        return QStringLiteral("DUE");
    if (a.snoozed)
        return QStringLiteral("SNOOZED");
    if (a.isDue())
        return QStringLiteral("DUE");
    return QStringLiteral("ACTIVE");
}

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
        std::cout << statusLabel(a).toStdString() << "  "
                  << remaining.toStdString() << "  "
                  << local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")).toStdString()
                  << "  [" << a.command.toStdString() << "]"
                  << (a.label.isEmpty() ? "" : (" " + a.label.toStdString()))
                  << "\n";
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("alarmqt"));
    QApplication::setApplicationDisplayName(QStringLiteral("AlarmQt"));
    QApplication::setApplicationVersion(QStringLiteral(ALARMQT_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Grumbel"));
    QApplication::setOrganizationDomain(QStringLiteral("grumbel.com"));
    QGuiApplication::setDesktopFileName(QStringLiteral("alarmqt"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/alarm.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Keyboard-friendly system-tray alarm / reminder"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("expression"),
                                 QStringLiteral("Alarm expression (e.g. \"in 5m kitchen\")"),
                                 QStringLiteral("[expression]"));
    parser.addOption({{"q", "quit"}, QStringLiteral("Quit the running instance")});
    parser.addOption({{"l", "list"},
                      QStringLiteral("List alarms (primary prints; secondary asks primary)")});
    parser.addOption({{"r", "raise"}, QStringLiteral("Raise the existing window")});
    parser.process(app);

    const QStringList pos = parser.positionalArguments();
    const bool wantQuit = parser.isSet(QStringLiteral("quit"));
    const bool wantList = parser.isSet(QStringLiteral("list"));
    const bool wantRaise = parser.isSet(QStringLiteral("raise"));

    QString message;
    if (wantQuit)
        message = QStringLiteral("--quit");
    else if (wantList)
        message = QStringLiteral("--list");
    else if (wantRaise)
        message = QStringLiteral("--raise");
    else if (!pos.isEmpty())
        message = pos.join(QLatin1Char(' '));
    else
        message = QStringLiteral("--raise");

    SingleInstance instance(kAppKey);

    if (!instance.isPrimary()) {
        if (wantList) {
            QString reply;
            if (SingleInstance::sendMessage(kAppKey, message, &reply)) {
                std::cout << reply.toStdString();
                if (!reply.isEmpty() && !reply.endsWith(QLatin1Char('\n')))
                    std::cout << '\n';
                return 0;
            }
            qWarning("Could not contact primary instance for --list");
            return 1;
        }
        if (SingleInstance::sendMessage(kAppKey, message)) {
            if (!wantQuit && !wantRaise && !pos.isEmpty())
                std::cout << "Sent alarm to running instance: " << message.toStdString() << "\n";
            return 0;
        }
        qWarning("Could not contact primary instance, starting anyway");
    }

    // Primary with --quit and nothing else useful: just exit
    if (wantQuit) {
        return 0;
    }

    AlarmManager manager;

    // --list only: print and exit without GUI
    if (wantList && pos.isEmpty() && !wantRaise) {
        printAlarmList(manager, true);
        return 0;
    }

    MainWindow window(&manager);

    QObject::connect(&instance, &SingleInstance::messageReceived,
                     &window, &MainWindow::handleExternalCommand);

    if (!pos.isEmpty()) {
        const QString expr = pos.join(QLatin1Char(' '));
        auto opt = AlarmManager::parse(expr);
        if (opt) {
            manager.add(*opt);
            const auto local = opt->triggerUtc.toLocalTime();
            std::cout << "Alarm added: " << opt->displayName().toStdString()
                      << "  at " << local.toString(Qt::ISODate).toStdString()
                      << "  (in " << opt->remainingString().toStdString() << ")\n";
        } else {
            std::cerr << "Could not parse alarm: " << expr.toStdString() << "\n"
                      << "Examples: in 5m kitchen | in 5m, tea | at 15:10 standup\n";
            return 1;
        }
    }

    if (wantList)
        printAlarmList(manager, true);

    window.show();
    return app.exec();
}
