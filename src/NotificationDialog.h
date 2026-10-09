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

/** How the ringing alarm is presented. */
enum class NotificationStyle {
    Standard = 0,  ///< Dialog with alternating red/black side strips (default)
    Simple = 1,    ///< Same dialog, no side strips
    Fullscreen = 2 ///< Full-screen overlay that flashes the whole display
};

class NotificationDialog : public QDialog {
    Q_OBJECT
public:
    explicit NotificationDialog(const Alarm& alarm, NotificationStyle style,
                                qreal volume = 0.9, QWidget* parent = nullptr);
    ~NotificationDialog() override;

    QUuid alarmId() const { return m_alarm.id; }
    NotificationStyle style() const { return m_style; }

signals:
    void acknowledged(const QUuid& id);
    void snoozed(const QUuid& id, int minutes);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void done(int r) override;

private:
    void buildStandardOrSimple();
    void buildFullscreen();
    void setBlinkPhase(bool on);
    void playSound();
    void stopAlert();

    Alarm m_alarm;
    NotificationStyle m_style = NotificationStyle::Standard;
    QLabel* m_title = nullptr;
    QLabel* m_when = nullptr;
    QLabel* m_subtitle = nullptr;
    QFrame* m_leftBlink = nullptr;
    QFrame* m_rightBlink = nullptr;
    QWidget* m_flashRoot = nullptr; // fullscreen background
    QTimer m_blinkTimer;
    QTimer m_soundTimer;
    bool m_blinkOn = true;
    bool m_closing = false;
    QSoundEffect* m_sound = nullptr;
};
