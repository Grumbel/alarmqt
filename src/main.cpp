// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmManager.h"
#include "MainWindow.h"
#include "SingleInstance.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <iostream>

static const QString kAppKey = QStringLiteral("alarmqt-single-instance-v1");

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("alarmqt"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("alarmqt"));
    QApplication::setQuitOnLastWindowClosed(false); // tray keeps us alive

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Simple system-tray alarm app"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("alarm"),
                                 QStringLiteral("Alarm expression, e.g. \"in 5m\" or \"at 15:10\""),
                                 QStringLiteral("[alarm]"));
    parser.addOption({{"q", "quit"}, QStringLiteral("Quit the running instance")});
    parser.addOption({{"l", "list"}, QStringLiteral("List active alarms (primary only)")});
    parser.process(app);

    const QStringList pos = parser.positionalArguments();
    QString message;
    if (parser.isSet(QStringLiteral("quit")))
        message = QStringLiteral("--quit");
    else if (parser.isSet(QStringLiteral("list")))
        message = QStringLiteral("--list");
    else if (!pos.isEmpty())
        message = pos.join(QLatin1Char(' '));
    else
        message = QStringLiteral("--raise");

    SingleInstance instance(kAppKey);

    if (!instance.isPrimary()) {
        // Secondary: forward and exit
        if (SingleInstance::sendMessage(kAppKey, message)) {
            if (message == QLatin1String("--list")) {
                // can't easily get reply; just inform
                std::cout << "Sent --list to primary instance (see its window / logs)\n";
            }
            return 0;
        }
        // Could not contact primary – fall through and become primary? rare race
        qWarning("Could not contact primary instance, starting anyway");
    }

    AlarmManager manager;
    MainWindow window(&manager);

    QObject::connect(&instance, &SingleInstance::messageReceived,
                     &window, &MainWindow::handleExternalCommand);

    // If started with an alarm expression, add it
    if (!pos.isEmpty() && message != QLatin1String("--raise")
        && message != QLatin1String("--quit") && message != QLatin1String("--list")) {
        auto opt = AlarmManager::parse(message);
        if (opt)
            manager.add(*opt);
        else
            qWarning() << "Could not parse alarm:" << message;
    }

    if (parser.isSet(QStringLiteral("list"))) {
        for (const auto& a : manager.alarms()) {
            if (a.acknowledged)
                continue;
            const auto local = a.triggerUtc.toLocalTime();
            std::cout << a.remainingString().toStdString() << "  "
                      << local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")).toStdString()
                      << "  " << a.label.toStdString() << "\n";
        }
    }

    window.show();
    return app.exec();
}
