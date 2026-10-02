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

static void printAlarmList(const AlarmManager& manager, bool includeDone) {
    for (const auto& a : manager.alarms()) {
        if (!includeDone && a.acknowledged)
            continue;
        const auto local = a.triggerUtc.toLocalTime();
        const char* st = a.acknowledged ? "DONE" : (a.isDue() ? "DUE" : "ACTIVE");
        std::cout << st << "  "
                  << (a.acknowledged ? "—" : a.remainingString().toStdString()) << "  "
                  << local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")).toStdString()
                  << "  [" << a.command.toStdString() << "]"
                  << (a.label.isEmpty() ? "" : (" " + a.label.toStdString()))
                  << "\n";
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("alarmqt"));
    QApplication::setApplicationVersion(QStringLiteral(ALARMQT_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Grumbel"));
    QApplication::setOrganizationDomain(QStringLiteral("alarmqt"));
    QApplication::setDesktopFileName(QStringLiteral("alarmqt"));
    QApplication::setQuitOnLastWindowClosed(false);

    const QIcon appIcon(QStringLiteral(":/icons/alarm.svg"));
    QApplication::setWindowIcon(appIcon);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Simple system-tray alarm app"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("alarm"),
                                 QStringLiteral("Alarm expression, e.g. \"in 5m\" or \"at 15:10\""),
                                 QStringLiteral("[alarm]"));
    parser.addOption({{"q", "quit"}, QStringLiteral("Quit the running instance")});
    parser.addOption({{"l", "list"}, QStringLiteral("List alarms (primary prints; secondary asks primary)")});
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
        if (SingleInstance::sendMessage(kAppKey, message)) {
            if (wantList)
                std::cout << "Sent --list to primary instance\n";
            else if (!wantQuit && !wantRaise && !pos.isEmpty())
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
                      << "Examples: in 5m | in 5m (kitchen) | at 15:10\n";
            return 1;
        }
    }

    if (wantList)
        printAlarmList(manager, true);

    window.show();
    return app.exec();
}
