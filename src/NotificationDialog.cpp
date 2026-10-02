// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NotificationDialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QSoundEffect>
#include <QUrl>
#include <QVBoxLayout>

NotificationDialog::NotificationDialog(const Alarm& alarm, QWidget* parent)
    : QDialog(parent)
    , m_alarm(alarm)
{
    setWindowTitle(tr("Alarm – %1").arg(alarm.displayName()));
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);
    setMinimumWidth(420);
    setMinimumHeight(160);

    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1a202c; color: #f7fafc; }"
        "QLabel { color: #f7fafc; background: transparent; }"
        "QPushButton { padding: 8px 14px; }"));

    auto* root = new QHBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto makeBlinker = []() {
        auto* f = new QFrame;
        f->setFixedWidth(18);
        f->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        f->setStyleSheet(QStringLiteral("QFrame { background-color: #2d3748; border: none; }"));
        return f;
    };
    m_leftBlink = makeBlinker();
    m_rightBlink = makeBlinker();

    auto* center = new QWidget;
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(20, 16, 20, 16);

    m_title = new QLabel(alarm.displayName());
    m_title->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    m_subtitle = new QLabel(tr("Time is up!"));
    m_subtitle->setAlignment(Qt::AlignCenter);
    m_subtitle->setStyleSheet(QStringLiteral("color: #a0aec0;"));

    centerLayout->addStretch(1);
    centerLayout->addWidget(m_title);
    centerLayout->addWidget(m_subtitle);
    centerLayout->addSpacing(12);

    auto* btnLayout = new QHBoxLayout;
    auto* snooze5 = new QPushButton(tr("Snooze 5m"));
    auto* snooze10 = new QPushButton(tr("Snooze 10m"));
    auto* ackBtn = new QPushButton(tr("Acknowledge (Enter)"));
    ackBtn->setDefault(true);
    btnLayout->addWidget(snooze5);
    btnLayout->addWidget(snooze10);
    btnLayout->addWidget(ackBtn);
    centerLayout->addLayout(btnLayout);
    centerLayout->addStretch(1);

    root->addWidget(m_leftBlink);
    root->addWidget(center, 1);
    root->addWidget(m_rightBlink);

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

    m_sound = new QSoundEffect(this);
    m_sound->setSource(QUrl(QStringLiteral("qrc:/sounds/alarm.wav")));
    m_sound->setVolume(0.9);
    m_sound->setLoopCount(1);

    connect(&m_blinkTimer, &QTimer::timeout, this, [this]() {
        if (m_closing)
            return;
        m_blinkOn = !m_blinkOn;
        setBlinkOn(m_blinkOn);
        if (m_blinkOn)
            playSound();
    });
    m_blinkTimer.start(700);
    setBlinkOn(true);
    playSound();

    if (auto* screen = QApplication::primaryScreen()) {
        const QRect geo = screen->availableGeometry();
        move(geo.center() - QPoint(width() / 2, height() / 2));
    }

    show();
    raise();
    activateWindow();
}

NotificationDialog::~NotificationDialog() {
    stopAlert();
}

void NotificationDialog::stopAlert() {
    m_closing = true;
    m_blinkTimer.stop();
    if (m_sound) {
        m_sound->stop();
    }
}

void NotificationDialog::done(int r) {
    stopAlert();
    QDialog::done(r);
}

void NotificationDialog::setBlinkOn(bool on) {
    const char* style = on
                            ? "QFrame { background-color: #fc8181; border: none; }"
                            : "QFrame { background-color: #2d3748; border: none; }";
    m_leftBlink->setStyleSheet(QString::fromUtf8(style));
    m_rightBlink->setStyleSheet(QString::fromUtf8(style));
}

void NotificationDialog::playSound() {
    if (m_closing)
        return;
    if (m_sound && m_sound->status() != QSoundEffect::Error) {
        m_sound->play();
        return;
    }
    QApplication::beep();
}

void NotificationDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit acknowledged(m_alarm.id);
        accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        emit snoozed(m_alarm.id, 5);
        accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void NotificationDialog::closeEvent(QCloseEvent* event) {
    if (!m_closing) {
        // Window manager close → treat as snooze (same as before)
        emit snoozed(m_alarm.id, 5);
    }
    stopAlert();
    event->accept();
}
