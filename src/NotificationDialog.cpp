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

namespace {
constexpr int kBlinkIntervalMs = 250;
constexpr int kSoundIntervalMs = 1500;
constexpr int kStripWidth = 48; // full-height side panels

const char* kStyleRed =
    "QFrame { background-color: #e53e3e; border: none; }";
const char* kStyleBlack =
    "QFrame { background-color: #000000; border: none; }";
} // namespace

NotificationDialog::NotificationDialog(const Alarm& alarm, QWidget* parent)
    : QDialog(parent)
    , m_alarm(alarm)
{
    setWindowTitle(tr("Alarm – %1").arg(alarm.displayName()));
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);
    setMinimumWidth(440);
    setMinimumHeight(180);

    // Default system grey background; only the full-height side strips flash.

    // Side strips flush to the dialog edges, full height.
    auto* root = new QHBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto makeStrip = [](QFrame** out) {
        auto* strip = new QFrame;
        strip->setFixedWidth(kStripWidth);
        strip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        strip->setStyleSheet(QString::fromUtf8(kStyleBlack));
        *out = strip;
        return strip;
    };

    auto* leftCol = makeStrip(&m_leftBlink);
    auto* rightCol = makeStrip(&m_rightBlink);

    auto* center = new QWidget;
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(16, 16, 16, 16);

    m_title = new QLabel(alarm.displayName());
    m_title->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);

    const QDateTime whenLocal = (alarm.scheduledUtc.isValid() ? alarm.scheduledUtc : alarm.triggerUtc)
                                    .toLocalTime();
    QString whenText = tr("When: %1").arg(
        whenLocal.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")));
    if (alarm.recurrence.isRecurring())
        whenText += tr("  ·  repeats %1").arg(alarm.recurrence.describe());
    m_when = new QLabel(whenText);
    m_when->setAlignment(Qt::AlignCenter);

    if (alarm.missed) {
        m_subtitle = new QLabel(tr("Missed — app was not running at the scheduled time"));
        m_subtitle->setStyleSheet(QStringLiteral("color: #c53030; font-weight: bold;"));
    } else {
        m_subtitle = new QLabel(tr("Time is up!"));
    }
    m_subtitle->setAlignment(Qt::AlignCenter);
    m_subtitle->setWordWrap(true);

    centerLayout->addStretch(1);
    centerLayout->addWidget(m_title);
    centerLayout->addWidget(m_when);
    centerLayout->addWidget(m_subtitle);
    centerLayout->addSpacing(12);

    auto* btnLayout = new QHBoxLayout;
    auto* snooze5 = new QPushButton(tr("Snooze 5m"));
    auto* snooze10 = new QPushButton(tr("Snooze 10m"));
    // Recurring alarms re-arm on acknowledge instead of becoming DONE.
    auto* ackBtn = new QPushButton(alarm.recurrence.isRecurring()
                                       ? tr("Acknowledge, repeat (Enter)")
                                       : tr("Acknowledge (Enter)"));
    ackBtn->setDefault(true);
    btnLayout->addWidget(snooze5);
    btnLayout->addWidget(snooze10);
    btnLayout->addWidget(ackBtn);
    centerLayout->addLayout(btnLayout);
    centerLayout->addStretch(1);

    root->addWidget(leftCol);
    root->addWidget(center, 1);
    root->addWidget(rightCol);

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
        m_leftRed = !m_leftRed;
        setBlinkPhase(m_leftRed);
    });
    m_blinkTimer.start(kBlinkIntervalMs);

    connect(&m_soundTimer, &QTimer::timeout, this, [this]() {
        if (!m_closing)
            playSound();
    });
    m_soundTimer.start(kSoundIntervalMs);

    setBlinkPhase(true);
    playSound();

    adjustSize();
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
    m_soundTimer.stop();
    if (m_sound) {
        m_sound->stop();
    }
}

void NotificationDialog::done(int r) {
    stopAlert();
    QDialog::done(r);
}

void NotificationDialog::setBlinkPhase(bool leftRed) {
    m_leftBlink->setStyleSheet(QString::fromUtf8(leftRed ? kStyleRed : kStyleBlack));
    m_rightBlink->setStyleSheet(QString::fromUtf8(leftRed ? kStyleBlack : kStyleRed));
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
    if (!m_closing && event->spontaneous()) {
        // Window manager close → treat as snooze. Programmatic closes (e.g.
        // application shutdown) leave the alarm due.
        emit snoozed(m_alarm.id, 5);
    }
    stopAlert();
    event->accept();
}
