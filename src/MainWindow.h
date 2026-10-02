#pragma once

#include "AlarmManager.h"
#include "NotificationDialog.h"

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QHash>

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
    void onAlarmTriggered(const Alarm& a);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void addFromInput();
    void removeSelected();
    void updateTray();

private:
    void createTray();
    void showNotification(const Alarm& a);

    AlarmManager* m_manager;
    QListWidget* m_list = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_status = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    QHash<QUuid, NotificationDialog*> m_dialogs;
};
