// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QIcon>
#include <QTimeZone>
#include <QTableWidgetItem>
#include <QBrush>
#include <QColor>
#include <QFont>

#include <algorithm>

namespace {
constexpr int kColStatus = 0;
constexpr int kColRemaining = 1;
constexpr int kColWhen = 2;
constexpr int kColCommand = 3;
constexpr int kColLabel = 4;
constexpr int kRoleId = Qt::UserRole;

QColor rowBackground(const Alarm& a) {
    if (a.acknowledged)
        return QColor(0x68, 0xd3, 0x91, 0x40); // soft green DONE
    if (a.triggered)
        return QColor(0xfc, 0x81, 0x81, 0x55); // soft red
    if (a.remainingMs() < 60'000)
        return QColor(0xf6, 0xe0, 0x5e, 0x55); // soft yellow
    return QColor();
}

QString statusText(const Alarm& a) {
    if (a.acknowledged)
        return QStringLiteral("DONE");
    if (a.triggered)
        return QStringLiteral("DUE");
    return QStringLiteral("ACTIVE");
}
} // namespace

MainWindow::MainWindow(AlarmManager* manager, QWidget* parent)
    : QMainWindow(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("AlarmQt"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/alarm.svg")));
    setMinimumSize(520, 360);
    resize(620, 440);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);

    // Prominent current time
    m_clock = new QLabel;
    m_clock->setAlignment(Qt::AlignCenter);
    QFont clockFont = font();
    clockFont.setPointSize(clockFont.pointSize() + 14);
    clockFont.setBold(true);
    m_clock->setFont(clockFont);
    m_clock->setStyleSheet(QStringLiteral("padding: 8px 4px;"));
    layout->addWidget(m_clock);
    updateClock();

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText(tr("in 5m (kitchen)  ·  at 15:10 \"standup\""));
    m_input->setClearButtonEnabled(true);
    auto* addBtn = new QPushButton(tr("Add"));
    auto* editBtn = new QPushButton(tr("Edit"));
    auto* restartBtn = new QPushButton(tr("Restart"));
    auto* removeBtn = new QPushButton(tr("Remove"));
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(addBtn);
    inputRow->addWidget(editBtn);
    inputRow->addWidget(restartBtn);
    inputRow->addWidget(removeBtn);
    layout->addLayout(inputRow);

    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::addFromInput);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addFromInput);
    connect(editBtn, &QPushButton::clicked, this, &MainWindow::editSelected);
    connect(restartBtn, &QPushButton::clicked, this, &MainWindow::restartSelected);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeSelected);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels(
        {tr("Status"), tr("Remaining"), tr("When"), tr("Command"), tr("Label")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(kColStatus, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColRemaining, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColWhen, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColCommand, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColLabel, QHeaderView::Stretch);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::StrongFocus);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MainWindow::onRowDoubleClicked);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &MainWindow::onTableContextMenu);

    m_status = new QLabel;
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    new QShortcut(QKeySequence::New, this, [this]() {
        m_input->setFocus();
        m_input->selectAll();
    });
    new QShortcut(QKeySequence::Delete, this, [this]() { removeSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this, [this]() { removeSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this, [this]() { editSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this, [this]() { restartSelected(); });
    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this]() { hide(); });

    connect(m_manager, &AlarmManager::alarmsChanged, this, &MainWindow::refreshList);
    connect(m_manager, &AlarmManager::alarmTriggered, this, &MainWindow::onAlarmTriggered);
    connect(m_manager, &AlarmManager::alarmAcknowledged, this, [this](const QUuid& id) {
        m_activeTriggered.remove(id);
        if (auto* d = m_dialogs.take(id))
            d->deleteLater();
    });

    // Clock + list refresh share the manager's 1 Hz tick via alarmsChanged,
    // but also tick the clock even when no alarms change.
    auto* clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    clockTimer->start(1000);

    m_renotifyTimer.setInterval(30'000);
    connect(&m_renotifyTimer, &QTimer::timeout, this, &MainWindow::renotifyTriggered);
    m_renotifyTimer.start();

    createTray();
    refreshList();
    m_input->setFocus();
}

void MainWindow::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();
    m_clock->setText(now.toString(QStringLiteral("dddd  yyyy-MM-dd  HH:mm:ss  t")));
}

void MainWindow::createTray() {
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(QStringLiteral(":/icons/alarm.svg")));
    m_tray->setToolTip(tr("AlarmQt"));

    auto* menu = new QMenu(this);
    menu->addAction(tr("Show / Hide"), this, [this]() {
        if (isVisible())
            hide();
        else
            raiseAndActivate();
    });
    menu->addAction(tr("Add alarm…"), this, [this]() {
        raiseAndActivate();
        m_input->setFocus();
    });
    menu->addAction(tr("Clear DONE alarms"), this, &MainWindow::clearDoneAlarms);
    menu->addSeparator();
    menu->addAction(tr("Quit"), qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible())
            hide();
        else
            raiseAndActivate();
    }
}

void MainWindow::raiseAndActivate() {
    show();
    raise();
    activateWindow();
    m_input->setFocus();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (!m_input->hasFocus())
            removeSelected();
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::addFromInput() {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;

    auto opt = AlarmManager::parse(text);
    if (!opt) {
        QMessageBox::warning(this, tr("Parse error"),
                             tr("Could not understand: “%1”\n\n"
                                "Examples:\n"
                                "  in 5m\n"
                                "  in 2h30m\n"
                                "  at 15:10\n"
                                "  at 2026-10-03 09:00\n"
                                "  in 5m (kitchen)\n"
                                "  in 10m \"tea\"")
                                 .arg(text));
        return;
    }
    m_manager->add(*opt);
    m_input->clear();
}

void MainWindow::removeSelected() {
    const auto ranges = m_table->selectionModel()->selectedRows();
    if (ranges.isEmpty())
        return;

    QList<QUuid> ids;
    for (const QModelIndex& idx : ranges) {
        auto* item = m_table->item(idx.row(), kColStatus);
        if (item)
            ids.append(item->data(kRoleId).toUuid());
    }
    if (ids.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this, tr("Remove alarms"),
        tr("Permanently remove %1 selected alarm(s)?").arg(ids.size()));
    if (answer != QMessageBox::Yes)
        return;

    for (const QUuid& id : ids) {
        m_manager->remove(id);
        m_activeTriggered.remove(id);
        if (auto* d = m_dialogs.take(id))
            d->deleteLater();
    }
}

void MainWindow::clearDoneAlarms() {
    const int n = std::count_if(m_manager->alarms().begin(), m_manager->alarms().end(),
                                [](const Alarm& a) { return a.acknowledged; });
    if (n == 0) {
        QMessageBox::information(this, tr("Clear DONE"), tr("No DONE alarms to clear."));
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Clear DONE"),
        tr("Permanently remove %1 DONE alarm(s)?").arg(n));
    if (answer != QMessageBox::Yes)
        return;
    m_manager->clearDone();
}

void MainWindow::restartSelected() {
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Restart"), tr("Select an alarm to restart."));
        return;
    }
    for (const QModelIndex& idx : rows) {
        auto* item = m_table->item(idx.row(), kColStatus);
        if (!item)
            continue;
        const QUuid id = item->data(kRoleId).toUuid();
        m_activeTriggered.remove(id);
        if (auto* d = m_dialogs.take(id))
            d->deleteLater();
        m_manager->restart(id);
    }
}

void MainWindow::onTableContextMenu(const QPoint& pos) {
    const QModelIndex index = m_table->indexAt(pos);
    if (index.isValid())
        m_table->selectRow(index.row());

    QMenu menu(this);
    menu.addAction(tr("Edit…"), this, &MainWindow::editSelected);
    menu.addAction(tr("Restart"), this, &MainWindow::restartSelected);
    menu.addSeparator();
    menu.addAction(tr("Remove"), this, &MainWindow::removeSelected);
    menu.addAction(tr("Clear all DONE"), this, &MainWindow::clearDoneAlarms);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MainWindow::onRowDoubleClicked(int row, int /*column*/) {
    auto* item = m_table->item(row, kColStatus);
    if (!item)
        return;
    editAlarm(item->data(kRoleId).toUuid());
}

void MainWindow::editSelected() {
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Edit"), tr("Select an alarm to edit."));
        return;
    }
    auto* item = m_table->item(rows.first().row(), kColStatus);
    if (!item)
        return;
    editAlarm(item->data(kRoleId).toUuid());
}

bool MainWindow::editAlarm(const QUuid& id) {
    Alarm* a = m_manager->alarmById(id);
    if (!a)
        return false;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit alarm"));
    auto* form = new QFormLayout(&dlg);

    auto* commandEdit = new QLineEdit(a->command);
    commandEdit->setPlaceholderText(tr("in 5m  ·  at 15:10"));
    auto* labelEdit = new QLineEdit(a->label);
    labelEdit->setPlaceholderText(tr("optional note"));
    auto* whenEdit = new QDateTimeEdit(a->triggerUtc.toLocalTime());
    whenEdit->setCalendarPopup(true);
    whenEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    whenEdit->setTimeZone(QTimeZone::systemTimeZone());

    auto* doneCheck = new QCheckBox(tr("Done (acknowledged)"));
    doneCheck->setChecked(a->acknowledged);

    form->addRow(tr("Command"), commandEdit);
    form->addRow(tr("Label"), labelEdit);
    form->addRow(tr("When"), whenEdit);
    form->addRow(QString(), doneCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    Alarm updated = *a;
    updated.command = commandEdit->text().trimmed();
    updated.label = labelEdit->text().trimmed();
    QDateTime local = whenEdit->dateTime();
    local.setTimeZone(QTimeZone::systemTimeZone());
    updated.triggerUtc = local.toUTC();
    updated.acknowledged = doneCheck->isChecked();
    if (updated.acknowledged)
        updated.triggered = false;
    else if (updated.triggerUtc > QDateTime::currentDateTimeUtc())
        updated.triggered = false;

    m_manager->update(updated);
    if (updated.acknowledged) {
        m_activeTriggered.remove(id);
        if (auto* d = m_dialogs.take(id))
            d->deleteLater();
    }
    return true;
}

void MainWindow::refreshList() {
    QSet<QUuid> selected;
    for (const QModelIndex& idx : m_table->selectionModel()->selectedRows()) {
        if (auto* item = m_table->item(idx.row(), kColStatus))
            selected.insert(item->data(kRoleId).toUuid());
    }

    m_table->setRowCount(0);

    // Active first (by time), then DONE at the bottom
    QVector<Alarm> active;
    QVector<Alarm> done;
    for (const auto& a : m_manager->alarms()) {
        if (a.acknowledged)
            done.append(a);
        else
            active.append(a);
    }

    auto addRow = [&](const Alarm& a) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        const QDateTime local = a.triggerUtc.toLocalTime();
        auto* status = new QTableWidgetItem(statusText(a));
        auto* remaining = new QTableWidgetItem(
            a.acknowledged ? QStringLiteral("—") : a.remainingString());
        auto* when = new QTableWidgetItem(
            local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")));
        auto* command = new QTableWidgetItem(a.command);
        auto* label = new QTableWidgetItem(a.label.isEmpty() ? QStringLiteral("—") : a.label);

        status->setData(kRoleId, a.id);
        remaining->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        status->setTextAlignment(Qt::AlignCenter);

        const QColor bg = rowBackground(a);
        for (QTableWidgetItem* item : {status, remaining, when, command, label}) {
            if (bg.isValid())
                item->setBackground(bg);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        }

        m_table->setItem(row, kColStatus, status);
        m_table->setItem(row, kColRemaining, remaining);
        m_table->setItem(row, kColWhen, when);
        m_table->setItem(row, kColCommand, command);
        m_table->setItem(row, kColLabel, label);

        if (selected.contains(a.id))
            m_table->selectRow(row);
    };

    for (const auto& a : active)
        addRow(a);
    for (const auto& a : done)
        addRow(a);

    updateTray();
    updateClock();

    if (auto next = m_manager->nextAlarm()) {
        const QDateTime local = next->triggerUtc.toLocalTime();
        m_status->setText(tr("Next: %1  (%2)  —  %3")
                              .arg(next->remainingString(),
                                   local.toString(QStringLiteral("HH:mm:ss")),
                                   next->displayName()));
    } else {
        m_status->setText(tr("No active alarms"));
    }
}

void MainWindow::updateTray() {
    if (auto next = m_manager->nextAlarm()) {
        m_tray->setToolTip(tr("AlarmQt – next in %1\n%2")
                               .arg(next->remainingString(), next->displayName()));
    } else {
        m_tray->setToolTip(tr("AlarmQt – no alarms"));
    }
}

void MainWindow::onAlarmTriggered(const Alarm& a) {
    m_activeTriggered.insert(a.id);
    showNotification(a);
    m_tray->showMessage(tr("Alarm"), a.displayName(), QSystemTrayIcon::Warning, 10'000);
}

void MainWindow::renotifyTriggered() {
    if (m_activeTriggered.isEmpty())
        return;

    for (const QUuid& id : std::as_const(m_activeTriggered)) {
        const Alarm* a = m_manager->alarmById(id);
        if (!a || a->acknowledged) {
            m_activeTriggered.remove(id);
            continue;
        }
        showNotification(*a);
        m_tray->showMessage(tr("Alarm (still active)"), a->displayName(),
                            QSystemTrayIcon::Warning, 8'000);
    }
}

void MainWindow::showNotification(const Alarm& a) {
    if (auto* existing = m_dialogs.value(a.id)) {
        existing->raise();
        existing->activateWindow();
        return;
    }

    auto* dlg = new NotificationDialog(a, this);
    m_dialogs.insert(a.id, dlg);

    connect(dlg, &NotificationDialog::acknowledged, this, [this](const QUuid& id) {
        m_manager->acknowledge(id);
        m_activeTriggered.remove(id);
        m_dialogs.remove(id);
    });
    connect(dlg, &NotificationDialog::snoozed, this, [this](const QUuid& id, int mins) {
        m_manager->snooze(id, mins);
        m_activeTriggered.remove(id);
        m_dialogs.remove(id);
    });
    connect(dlg, &QObject::destroyed, this, [this, id = a.id]() {
        m_dialogs.remove(id);
    });
}

void MainWindow::handleExternalCommand(const QString& cmd) {
    const QString c = cmd.trimmed();
    if (c == QLatin1String("--raise") || c.isEmpty()) {
        raiseAndActivate();
        return;
    }
    if (c == QLatin1String("--quit")) {
        qApp->quit();
        return;
    }
    if (c == QLatin1String("--list")) {
        return;
    }

    auto opt = AlarmManager::parse(c);
    if (opt) {
        m_manager->add(*opt);
        raiseAndActivate();
        m_tray->showMessage(tr("Alarm added"), opt->displayName(),
                            QSystemTrayIcon::Information, 3'000);
    } else {
        m_tray->showMessage(tr("Parse error"),
                            tr("Could not parse: %1").arg(c),
                            QSystemTrayIcon::Warning, 5'000);
    }
}
