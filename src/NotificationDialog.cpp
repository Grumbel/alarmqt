// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NotificationDialog.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>

NotificationDialog::NotificationDialog(const Alarm& alarm, QWidget* parent)
    : QDialog(parent)
    , m_alarm(alarm)
{
    setWindowTitle(tr("Alarm – %1").arg(alarm.label));
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    setModal(false);
    setMinimumWidth(360);

    auto* layout = new QVBoxLayout(this);

    m_label = new QLabel(tr("<h2>%1</h2><p>Time is up!</p>").arg(alarm.label.toHtmlEscaped()));
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setTextFormat(Qt::RichText);
    layout->addWidget(m_label);

    auto* btnLayout = new QHBoxLayout;
    auto* ackBtn = new QPushButton(tr("Acknowledge (Enter)"));
    ackBtn->setDefault(true);
    auto* snooze5 = new QPushButton(tr("Snooze 5m"));
    auto* snooze10 = new QPushButton(tr("Snooze 10m"));

    btnLayout->addWidget(snooze5);
    btnLayout->addWidget(snooze10);
    btnLayout->addWidget(ackBtn);
    layout->addLayout(btnLayout);

    connect(ackBtn, &QPushButton::clicked, this, [this]() {
        emit acknowledged(m_alarm.id);
        accept();
    });
    connect(snooze5, &QPushButton::clicked, this, [this]() {
        emit snoozed(m_alarm.id, 5);
        accept();
    });
    connect(snooze10, &QPushButton::clicked, this, [this]() {
        emit snoozed(m_alarm.id, 10);
        accept();
    });

    // Flash background to grab attention
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        m_flash = !m_flash;
        setStyleSheet(m_flash
                          ? QStringLiteral("QDialog { background-color: #742a2a; color: white; }")
                          : QStringLiteral("QDialog { background-color: #1a202c; color: white; }"));
    });
    m_flashTimer.start(800);

    // Center on primary screen
    if (auto* screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        move(geo.center() - rect().center());
    }

    // Raise & activate
    show();
    raise();
    activateWindow();
}

void NotificationDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit acknowledged(m_alarm.id);
        accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        // Treat Escape as snooze 5 so it doesn't just vanish
        emit snoozed(m_alarm.id, 5);
        accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void NotificationDialog::closeEvent(QCloseEvent* event) {
    // Closing the window = snooze, never silent discard
    emit snoozed(m_alarm.id, 5);
    event->accept();
}
