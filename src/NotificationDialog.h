// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Alarm.h"

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>

class NotificationDialog : public QDialog {
    Q_OBJECT
public:
    explicit NotificationDialog(const Alarm& alarm, QWidget* parent = nullptr);

    QUuid alarmId() const { return m_alarm.id; }

signals:
    void acknowledged(const QUuid& id);
    void snoozed(const QUuid& id, int minutes);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    Alarm m_alarm;
    QLabel* m_label = nullptr;
    QTimer m_flashTimer;
    bool m_flash = false;
};
