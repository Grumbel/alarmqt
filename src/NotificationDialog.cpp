// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NotificationDialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QPainter>
#include <QScreen>
#include <QSoundEffect>
#include <QSvgRenderer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int kBlinkIntervalMs = 250;
constexpr int kSoundIntervalMs = 1500;
constexpr int kStripWidth = 48;

const char* kStyleRed = "QFrame { background-color: #e53e3e; border: none; }";
const char* kStyleBlack = "QFrame { background-color: #000000; border: none; }";
const char* kFullscreenRed =
    "QWidget#flashRoot { background-color: #e53e3e; }";
const char* kFullscreenBlack =
    "QWidget#flashRoot { background-color: #1a0000; }";

class RingingClock : public QWidget {
public:
    explicit RingingClock(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_renderer(QStringLiteral(":/icons/alarm-ringing.svg"), this)
    {
        setFixedSize(m_renderer.defaultSize() * 3 / 4);
        connect(&m_renderer, &QSvgRenderer::repaintNeeded, this, qOverload<>(&QWidget::update));
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        m_renderer.render(&p, rect());
    }

private:
    QSvgRenderer m_renderer;
};

QWidget* buildContentColumn(const Alarm& alarm, QLabel** titleOut, QLabel** whenOut,
                            QLabel** subtitleOut, QWidget* parent,
                            bool large)
{
    auto* center = new QWidget(parent);
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(large ? 32 : 16, large ? 32 : 16, large ? 32 : 16,
                                     large ? 32 : 16);

    auto* title = new QLabel(alarm.displayName());
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + (large ? 14 : 6));
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setWordWrap(true);
    if (large)
        title->setStyleSheet(QStringLiteral("color: #ffffff;"));

    const QDateTime whenLocal = (alarm.scheduledUtc.isValid() ? alarm.scheduledUtc : alarm.triggerUtc)
                                    .toLocalTime();
    QString whenText = QObject::tr("When: %1").arg(
        whenLocal.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (alarm.recurrence.isRecurring())
        whenText += QObject::tr("  ·  repeats %1").arg(alarm.recurrence.describe());
    auto* when = new QLabel(whenText);
    when->setAlignment(Qt::AlignCenter);
    if (large)
        when->setStyleSheet(QStringLiteral("color: #ffe4e4; font-size: 16px;"));

    QLabel* subtitle = nullptr;
    if (alarm.missed) {
        subtitle = new QLabel(QObject::tr("Missed — app was not running at the scheduled time"));
        subtitle->setStyleSheet(large
                                    ? QStringLiteral("color: #ffd0d0; font-weight: bold; font-size: 18px;")
                                    : QStringLiteral("color: #c53030; font-weight: bold;"));
    } else {
        subtitle = new QLabel(QObject::tr("Time is up!"));
        if (large)
            subtitle->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 22px; font-weight: bold;"));
    }
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    centerLayout->addStretch(1);
    auto* clock = new RingingClock;
    if (large) {
        const QSize base = clock->sizeHint().isEmpty() ? QSize(96, 96) : clock->sizeHint();
        clock->setFixedSize(base * 3 / 2);
    }
    centerLayout->addWidget(clock, 0, Qt::AlignHCenter);
    centerLayout->addWidget(title);
    centerLayout->addWidget(when);
    centerLayout->addWidget(subtitle);
    centerLayout->addStretch(1);

    *titleOut = title;
    *whenOut = when;
    *subtitleOut = subtitle;
    return center;
}

QHBoxLayout* buildButtons(QWidget* parent, NotificationDialog* dlg, const Alarm& alarm)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(12);

    auto* ack = new QPushButton(QObject::tr("Acknowledge"), parent);
    ack->setDefault(true);
    ack->setMinimumHeight(36);
    auto* snooze5 = new QPushButton(QObject::tr("Snooze 5m"), parent);
    auto* snooze10 = new QPushButton(QObject::tr("Snooze 10m"), parent);
    snooze5->setMinimumHeight(36);
    snooze10->setMinimumHeight(36);

    QObject::connect(ack, &QPushButton::clicked, dlg, [dlg, id = alarm.id]() {
        emit dlg->acknowledged(id);
        dlg->accept();
    });
    QObject::connect(snooze5, &QPushButton::clicked, dlg, [dlg, id = alarm.id]() {
        emit dlg->snoozed(id, 5);
        dlg->accept();
    });
    QObject::connect(snooze10, &QPushButton::clicked, dlg, [dlg, id = alarm.id]() {
        emit dlg->snoozed(id, 10);
        dlg->accept();
    });

    row->addStretch(1);
    row->addWidget(ack);
    row->addWidget(snooze5);
    row->addWidget(snooze10);
    row->addStretch(1);
    return row;
}
} // namespace

NotificationDialog::NotificationDialog(const Alarm& alarm, NotificationStyle style,
                                       QWidget* parent)
    : QDialog(parent)
    , m_alarm(alarm)
    , m_style(style)
{
    setWindowTitle(tr("Alarm – %1").arg(alarm.displayName()));
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    if (m_style == NotificationStyle::Fullscreen)
        buildFullscreen();
    else
        buildStandardOrSimple();

    m_sound = new QSoundEffect(this);
    m_sound->setSource(QUrl(QStringLiteral("qrc:/sounds/alarm.wav")));
    m_sound->setVolume(0.9);
    m_sound->setLoopCount(1);

    connect(&m_blinkTimer, &QTimer::timeout, this, [this]() {
        if (m_closing)
            return;
        m_blinkOn = !m_blinkOn;
        setBlinkPhase(m_blinkOn);
    });
    m_blinkTimer.start(kBlinkIntervalMs);

    connect(&m_soundTimer, &QTimer::timeout, this, [this]() {
        if (!m_closing)
            playSound();
    });
    m_soundTimer.start(kSoundIntervalMs);

    setBlinkPhase(true);
    playSound();

    if (m_style == NotificationStyle::Fullscreen) {
        showFullScreen();
    } else {
        adjustSize();
        if (auto* screen = QApplication::primaryScreen()) {
            const QRect geo = screen->availableGeometry();
            move(geo.center() - QPoint(width() / 2, height() / 2));
        }
        show();
    }
    raise();
    activateWindow();
}

void NotificationDialog::buildStandardOrSimple()
{
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    setMinimumWidth(440);
    setMinimumHeight(180);

    auto* root = new QHBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    const bool withStrips = (m_style == NotificationStyle::Standard);

    if (withStrips) {
        auto makeStrip = [](QFrame** out) {
            auto* strip = new QFrame;
            strip->setFixedWidth(kStripWidth);
            strip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            strip->setStyleSheet(QString::fromUtf8(kStyleBlack));
            *out = strip;
            return strip;
        };
        root->addWidget(makeStrip(&m_leftBlink));
    }

    auto* center = buildContentColumn(m_alarm, &m_title, &m_when, &m_subtitle, this, false);
    auto* centerLayout = qobject_cast<QVBoxLayout*>(center->layout());
    centerLayout->addLayout(buildButtons(center, this, m_alarm));
    root->addWidget(center, 1);

    if (withStrips)
        root->addWidget([&]() {
            auto* strip = new QFrame;
            strip->setFixedWidth(kStripWidth);
            strip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            strip->setStyleSheet(QString::fromUtf8(kStyleBlack));
            m_rightBlink = strip;
            return strip;
        }());
}

void NotificationDialog::buildFullscreen()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowState(Qt::WindowFullScreen);

    m_flashRoot = new QWidget(this);
    m_flashRoot->setObjectName(QStringLiteral("flashRoot"));
    m_flashRoot->setStyleSheet(QString::fromUtf8(kFullscreenBlack));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_flashRoot);

    auto* root = new QVBoxLayout(m_flashRoot);
    root->setContentsMargins(40, 40, 40, 40);

    auto* center = buildContentColumn(m_alarm, &m_title, &m_when, &m_subtitle, m_flashRoot, true);
    root->addWidget(center, 1);

    auto* btnWrap = new QWidget(m_flashRoot);
    btnWrap->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 16px; min-width: 140px; padding: 10px 18px; }"));
    auto* btnLayout = new QHBoxLayout(btnWrap);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    auto* buttons = buildButtons(btnWrap, this, m_alarm);
    btnLayout->addLayout(buttons);
    root->addWidget(btnWrap);
}

NotificationDialog::~NotificationDialog() {
    stopAlert();
}

void NotificationDialog::stopAlert() {
    m_closing = true;
    m_blinkTimer.stop();
    m_soundTimer.stop();
    if (m_sound)
        m_sound->stop();
}

void NotificationDialog::done(int r) {
    stopAlert();
    QDialog::done(r);
}

void NotificationDialog::setBlinkPhase(bool on) {
    if (m_style == NotificationStyle::Fullscreen && m_flashRoot) {
        m_flashRoot->setStyleSheet(
            QString::fromUtf8(on ? kFullscreenRed : kFullscreenBlack));
        return;
    }
    if (m_style == NotificationStyle::Standard && m_leftBlink && m_rightBlink) {
        m_leftBlink->setStyleSheet(QString::fromUtf8(on ? kStyleRed : kStyleBlack));
        m_rightBlink->setStyleSheet(QString::fromUtf8(on ? kStyleBlack : kStyleRed));
    }
    // Simple: no visual blink strips
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
        emit snoozed(m_alarm.id, 5);
    }
    stopAlert();
    event->accept();
}
