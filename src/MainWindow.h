// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "AlarmManager.h"
#include "NotificationDialog.h"

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QHash>
#include <QTimer>
#include <QSet>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AlarmManager* manager, QWidget* parent = nullptr);

public slots:
    void raiseAndActivate();
    void handleExternalCommand(const QString& cmd);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void refreshList();
    void updateClock();
    void onAlarmTriggered(const Alarm& a);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void addFromInput();
    void removeSelected();
    void editSelected();
    void restartSelected();
    void clearDoneAlarms();
    void onRowDoubleClicked(int row, int column);
    void onTableContextMenu(const QPoint& pos);
    void updateTray();
    void renotifyTriggered();

private:
    void createTray();
    void showNotification(const Alarm& a);
    bool editAlarm(const QUuid& id);

    AlarmManager* m_manager;
    QLabel* m_clockIcon = nullptr;
    QLabel* m_clock = nullptr;
    QTableWidget* m_table = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_status = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    QHash<QUuid, NotificationDialog*> m_dialogs;
    QSet<QUuid> m_activeTriggered;
    QTimer m_renotifyTimer;
};
