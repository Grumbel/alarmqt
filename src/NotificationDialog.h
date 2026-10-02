// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Alarm.h"

#include <QDialog>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>

class QSoundEffect;

class NotificationDialog : public QDialog {
    Q_OBJECT
public:
    explicit NotificationDialog(const Alarm& alarm, QWidget* parent = nullptr);
    ~NotificationDialog() override;

    QUuid alarmId() const { return m_alarm.id; }

signals:
    void acknowledged(const QUuid& id);
    void snoozed(const QUuid& id, int minutes);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void done(int r) override;

private:
    void setBlinkOn(bool on);
    void playSound();
    void stopAlert();

    Alarm m_alarm;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QFrame* m_leftBlink = nullptr;
    QFrame* m_rightBlink = nullptr;
    QTimer m_blinkTimer;
    bool m_blinkOn = false;
    bool m_closing = false;
    QSoundEffect* m_sound = nullptr;
};
