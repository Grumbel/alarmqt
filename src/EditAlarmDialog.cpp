// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EditAlarmDialog.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {

QString formatLocal(const QDateTime& utc) {
    return QLocale().toString(utc.toLocalTime(), QStringLiteral("ddd yyyy-MM-dd HH:mm:ss"));
}

QString colored(const QString& text, const char* color) {
    return QStringLiteral("<span style=\"color:%1\">%2</span>")
        .arg(QLatin1String(color), text);
}

} // namespace

EditAlarmDialog::EditAlarmDialog(const Alarm& alarm, QWidget* parent)
    : QDialog(parent)
    , m_alarm(alarm) {
    setWindowTitle(tr("Edit alarm"));

    // --- Time expression + date picker -------------------------------------
    m_timeEdit = new QLineEdit(m_alarm.command);
    m_timeEdit->setPlaceholderText(tr("in 5m  ·  at 15:10  ·  every monday at 18:00"));
    m_timeEdit->setToolTip(tr(
        "Same syntax as the main input, e.g.\n"
        "  in 5m · in 1h30 · at 15:10 · tomorrow 9:00\n"
        "  next friday 5pm · 2026-12-24 18:00\n"
        "  every 30m · every weekday at 9:00\n"
        "A trailing note moves to Label; leading style words\n"
        "(silent, simple, max, notify, disabled) set the fields below."));
    m_timeEdit->setMinimumWidth(m_timeEdit->fontMetrics().averageCharWidth() * 44);

    auto* pickButton = new QToolButton;
    const QIcon calIcon = QIcon::fromTheme(QStringLiteral("x-office-calendar"),
                                           QIcon::fromTheme(QStringLiteral("view-calendar")));
    if (calIcon.isNull())
        pickButton->setText(tr("…"));
    else
        pickButton->setIcon(calIcon);
    pickButton->setToolTip(tr("Pick a date and time"));
    pickButton->setPopupMode(QToolButton::InstantPopup);

    auto* pickMenu = new QMenu(pickButton);
    auto* pickWidget = new QWidget;
    auto* pickLayout = new QVBoxLayout(pickWidget);
    m_pickDate = new QCalendarWidget;
    m_pickDate->setGridVisible(false);
    m_pickTime = new QTimeEdit;
    m_pickTime->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    auto* pickApply = new QPushButton(tr("Set"));
    auto* pickRow = new QHBoxLayout;
    pickRow->addWidget(new QLabel(tr("Time:")));
    pickRow->addWidget(m_pickTime, 1);
    pickRow->addWidget(pickApply);
    pickLayout->addWidget(m_pickDate);
    pickLayout->addLayout(pickRow);
    auto* pickAction = new QWidgetAction(pickMenu);
    pickAction->setDefaultWidget(pickWidget);
    pickMenu->addAction(pickAction);
    pickButton->setMenu(pickMenu);
    connect(pickMenu, &QMenu::aboutToShow, this, &EditAlarmDialog::preparePicker);
    connect(pickApply, &QPushButton::clicked, this, [this, pickMenu]() {
        applyPicker();
        pickMenu->close();
    });
    // Enter / double-click on a day picks it with the chosen time.
    connect(m_pickDate, &QCalendarWidget::activated, this, [this, pickMenu]() {
        applyPicker();
        pickMenu->close();
    });

    auto* timeRow = new QHBoxLayout;
    timeRow->setContentsMargins(0, 0, 0, 0);
    timeRow->addWidget(m_timeEdit, 1);
    timeRow->addWidget(pickButton);

    m_preview = new QLabel;
    m_preview->setTextFormat(Qt::RichText);
    m_preview->setWordWrap(true);
    m_preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // Room for the error + examples so the dialog does not jump while typing.
    m_preview->setMinimumHeight(m_preview->fontMetrics().lineSpacing() * 3);

    // --- Label ---------------------------------------------------------------
    m_labelEdit = new QLineEdit(m_alarm.label);
    m_labelEdit->setPlaceholderText(tr("optional note"));

    // --- Notification style --------------------------------------------------
    m_styleCombo = new QComboBox;
    // itemData: -1 = app default; else NotificationStyle int
    m_styleCombo->addItem(tr("Default (Settings)"), -1);
    m_styleCombo->addItem(tr("Standard (side blinkers)"), int(NotificationStyle::Standard));
    m_styleCombo->addItem(tr("Simple (no side blinkers)"), int(NotificationStyle::Simple));
    m_styleCombo->addItem(tr("Fullscreen flash"), int(NotificationStyle::Fullscreen));
    m_styleCombo->addItem(tr("Silent (blink only)"), int(NotificationStyle::Silent));
    m_styleCombo->addItem(tr("Notification only (tray)"), int(NotificationStyle::NotifyOnly));
    m_styleCombo->setCurrentIndex(std::max(
        0, m_styleCombo->findData(m_alarm.notificationStyle ? int(*m_alarm.notificationStyle) : -1)));

    // --- State ---------------------------------------------------------------
    m_stateCombo = new QComboBox;
    m_stateCombo->addItem(tr("Active"), int(AlarmState::Active));
    m_stateCombo->addItem(tr("Disabled (paused)"), int(AlarmState::Disabled));
    m_stateCombo->addItem(tr("Done (acknowledged)"), int(AlarmState::Done));
    m_stateCombo->setCurrentIndex(m_stateCombo->findData(int(alarmState(m_alarm))));

    auto* form = new QFormLayout;
    form->addRow(tr("&Time:"), timeRow);
    form->addRow(QString(), m_preview);
    form->addRow(tr("&Label:"), m_labelEdit);
    form->addRow(tr("&Notification:"), m_styleCombo);
    form->addRow(tr("&State:"), m_stateCombo);
    qobject_cast<QLabel*>(form->labelForField(timeRow))->setBuddy(m_timeEdit);

    // Standard roles: platform button order (GNOME Cancel|Ok, Windows Ok|Cancel).
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
        normalizeExpression();
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch(1);
    layout->addWidget(m_buttons);

    connect(m_timeEdit, &QLineEdit::textChanged, this, &EditAlarmDialog::updatePreview);
    connect(m_timeEdit, &QLineEdit::editingFinished, this, &EditAlarmDialog::normalizeExpression);
    connect(m_labelEdit, &QLineEdit::textChanged, this, &EditAlarmDialog::updatePreview);
    connect(m_stateCombo, &QComboBox::currentIndexChanged, this, &EditAlarmDialog::updatePreview);

    // Relative expressions ("in 5m") and the countdown move with the clock.
    m_refresh.setInterval(1000);
    connect(&m_refresh, &QTimer::timeout, this, &EditAlarmDialog::updatePreview);
    m_refresh.start();

    updatePreview();
    m_timeEdit->setFocus();
    m_timeEdit->selectAll();
}

AlarmEdit EditAlarmDialog::edit() const {
    AlarmEdit e;
    e.expression = m_timeEdit->text().trimmed();
    e.label = m_labelEdit->text().trimmed();
    e.state = static_cast<AlarmState>(m_stateCombo->currentData().toInt());
    const int style = m_styleCombo->currentData().toInt();
    if (style >= 0)
        e.style = static_cast<NotificationStyle>(style);
    return e;
}

void EditAlarmDialog::normalizeExpression() {
    // Only touch text the user changed; the stored command stays as-is.
    if (m_timeEdit->text().trimmed() == m_alarm.command.trimmed())
        return;
    AlarmEdit e = edit();
    if (!e.normalize())
        return;
    if (e.expression != m_timeEdit->text())
        m_timeEdit->setText(e.expression);
    if (e.label != m_labelEdit->text())
        m_labelEdit->setText(e.label);
    m_styleCombo->setCurrentIndex(std::max(0, m_styleCombo->findData(e.style ? int(*e.style) : -1)));
    m_stateCombo->setCurrentIndex(m_stateCombo->findData(int(e.state)));
}

void EditAlarmDialog::updatePreview() {
    const AlarmEditOutcome out = applyAlarmEdit(m_alarm, m_alarm.command, edit());
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(out.alarm.has_value());

    // Repeating alarms are never DONE (acknowledge re-arms them).
    const bool recurring = out.alarm ? out.alarm->recurrence.isRecurring()
                                     : m_alarm.recurrence.isRecurring();
    if (auto* model = qobject_cast<QStandardItemModel*>(m_stateCombo->model())) {
        if (auto* doneItem = model->item(m_stateCombo->findData(int(AlarmState::Done)))) {
            const bool allowDone = !recurring || m_stateCombo->currentData().toInt()
                                                     == int(AlarmState::Done);
            doneItem->setEnabled(allowDone);
        }
    }

    if (!out.alarm) {
        m_preview->setText(colored(out.error.toHtmlEscaped(), "#d03030") + QStringLiteral("<br><small>")
                           + tr("Try: in 5m · at 15:10 · tomorrow 9:00 · "
                                "2026-12-24 18:00 · every monday at 18:00")
                           + QStringLiteral("</small>"));
        return;
    }

    const Alarm& a = *out.alarm;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QDateTime when = a.scheduledUtc.isValid() ? a.scheduledUtc : a.triggerUtc;
    const QString whenText = QStringLiteral("<b>%1</b>").arg(formatLocal(when).toHtmlEscaped());

    QString line;
    switch (alarmState(a)) {
    case AlarmState::Done:
        line = tr("%1 · done").arg(whenText);
        break;
    case AlarmState::Disabled:
        line = tr("%1 · paused").arg(whenText);
        break;
    case AlarmState::Active:
        if (a.triggerUtc <= nowUtc)
            line = tr("%1 · %2").arg(whenText, colored(tr("overdue, rings now"), "#d03030"));
        else if (a.snoozed)
            line = tr("%1 · snoozed, rings in %2").arg(whenText, a.remainingString(nowUtc));
        else
            line = tr("%1 · in %2").arg(whenText, a.remainingString(nowUtc));
        break;
    }

    QStringList details;
    if (a.recurrence.isRecurring())
        details << tr("repeats %1").arg(a.recurrence.describe()).toHtmlEscaped();
    if (!out.rescheduled)
        details << tr("schedule unchanged");
    if (!details.isEmpty())
        line += QStringLiteral("<br><small>%1</small>")
                    .arg(details.join(QStringLiteral(" · ")));
    m_preview->setText(line);
}

void EditAlarmDialog::preparePicker() {
    const AlarmEditOutcome out = applyAlarmEdit(m_alarm, m_alarm.command, edit());
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    QDateTime start;
    if (out.alarm && out.alarm->scheduledUtc.isValid() && out.alarm->scheduledUtc > nowUtc) {
        start = out.alarm->scheduledUtc.toLocalTime();
    } else {
        // Next full hour from now
        const QDateTime now = QDateTime::currentDateTime();
        start = QDateTime(now.date(), QTime(now.time().hour(), 0)).addSecs(3600);
    }
    m_pickDate->setSelectedDate(start.date());
    m_pickTime->setTime(start.time());
    m_pickDate->setFocus();
}

void EditAlarmDialog::applyPicker() {
    const QTime t = m_pickTime->time();
    const QString time = t.toString(t.second() ? QStringLiteral("HH:mm:ss")
                                               : QStringLiteral("HH:mm"));
    m_timeEdit->setText(m_pickDate->selectedDate().toString(QStringLiteral("yyyy-MM-dd"))
                        + QLatin1Char(' ') + time);
    m_timeEdit->setFocus();
}
