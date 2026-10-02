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

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("alarmqt"));
    QApplication::setApplicationVersion(QStringLiteral(ALARMQT_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Grumbel"));
    QApplication::setOrganizationDomain(QStringLiteral("alarmqt"));
    QApplication::setDesktopFileName(QStringLiteral("alarmqt"));
    QApplication::setQuitOnLastWindowClosed(false); // tray keeps us alive

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
    parser.addOption({{"l", "list"}, QStringLiteral("List active alarms (primary only)")});
    parser.addOption({{"r", "raise"}, QStringLiteral("Raise the existing window")});
    parser.process(app);

    const QStringList pos = parser.positionalArguments();
    QString message;
    if (parser.isSet(QStringLiteral("quit")))
        message = QStringLiteral("--quit");
    else if (parser.isSet(QStringLiteral("list")))
        message = QStringLiteral("--list");
    else if (parser.isSet(QStringLiteral("raise")))
        message = QStringLiteral("--raise");
    else if (!pos.isEmpty())
        message = pos.join(QLatin1Char(' '));
    else
        message = QStringLiteral("--raise");

    SingleInstance instance(kAppKey);

    if (!instance.isPrimary()) {
        if (SingleInstance::sendMessage(kAppKey, message)) {
            if (message == QLatin1String("--list")) {
                std::cout << "Sent --list to primary instance\n";
            } else if (message != QLatin1String("--raise")
                       && message != QLatin1String("--quit")) {
                std::cout << "Sent alarm to running instance: "
                          << message.toStdString() << "\n";
            }
            return 0;
        }
        qWarning("Could not contact primary instance, starting anyway");
    }

    AlarmManager manager;
    MainWindow window(&manager);

    QObject::connect(&instance, &SingleInstance::messageReceived,
                     &window, &MainWindow::handleExternalCommand);

    // Cold-start with an alarm expression on the command line
    if (!pos.isEmpty()
        && !parser.isSet(QStringLiteral("quit"))
        && !parser.isSet(QStringLiteral("list"))
        && !parser.isSet(QStringLiteral("raise"))) {
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
                      << "Examples: in 5m | in 2h30m | at 15:10 | at 2026-10-03 09:00\n";
            return 1;
        }
    }

    if (parser.isSet(QStringLiteral("list"))) {
        for (const auto& a : manager.alarms()) {
            if (a.acknowledged)
                continue;
            const auto local = a.triggerUtc.toLocalTime();
            std::cout << (a.acknowledged ? "DONE" : a.remainingString().toStdString()) << "  "
                      << local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")).toStdString()
                      << "  [" << a.command.toStdString() << "]"
                      << (a.label.isEmpty() ? "" : (" " + a.label.toStdString()))
                      << "\n";
        }
    }

    window.show();
    return app.exec();
}
