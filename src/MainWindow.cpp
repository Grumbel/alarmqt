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
#include <QItemSelection>
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

    // Prominent current time with app icon on the left
    auto* clockRow = new QHBoxLayout;
    clockRow->setSpacing(16);
    clockRow->setContentsMargins(8, 4, 8, 4);

    m_clockIcon = new QLabel;
    m_clockIcon->setFixedSize(72, 72);
    m_clockIcon->setAlignment(Qt::AlignCenter);
    m_clockIcon->setScaledContents(false);
    {
        const QIcon icon(QStringLiteral(":/icons/alarm.svg"));
        m_clockIcon->setPixmap(icon.pixmap(QSize(72, 72)));
    }

    m_clock = new QLabel;
    m_clock->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    QFont clockFont = font();
    clockFont.setPointSize(clockFont.pointSize() + 14);
    clockFont.setBold(true);
    m_clock->setFont(clockFont);
    m_clock->setStyleSheet(QStringLiteral("padding: 4px 0;"));

    clockRow->addWidget(m_clockIcon, 0, Qt::AlignVCenter);
    clockRow->addWidget(m_clock, 1, Qt::AlignVCenter);
    layout->addLayout(clockRow);
    updateClock();

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText(tr("in 5m kitchen  ·  in 5m, tea  ·  at 15:10 standup"));
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

MainWindow::~MainWindow() {
    // Notification dialogs are child widgets and only get deleted by the
    // QWidget base destructor, after our members are gone. Cut their
    // connections first so their destroyed() handler cannot touch m_dialogs.
    for (auto* dlg : findChildren<NotificationDialog*>())
        disconnect(dlg, nullptr, this, nullptr);
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
    // exit() rather than quit(): quit() first sends close events, which the
    // notification dialogs would treat as "snooze".
    menu->addAction(tr("Quit"), qApp, []() { QApplication::exit(0); });
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
                                "  in 5m kitchen\n"
                                "  in 5m, tea\n"
                                "  at 15:10 standup\n"
                                "  in 2h30m\n"
                                "  at 2026-10-03 09:00")
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
    const auto& alarms = m_manager->alarms();
    const auto n = std::count_if(alarms.begin(), alarms.end(),
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
    // Work on a copy: dlg.exec() runs a nested event loop during which the
    // manager may reallocate or drop its alarms (tick, CLI add, remove, ...).
    const Alarm* current = m_manager->alarmById(id);
    if (!current)
        return false;
    const Alarm original = *current;
    const Alarm* a = &original;

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

    auto* applyCmdBtn = new QPushButton(tr("Apply command → When"));
    form->addRow(QString(), applyCmdBtn);
    QObject::connect(applyCmdBtn, &QPushButton::clicked, &dlg, [this, commandEdit, labelEdit, whenEdit, &dlg]() {
        const QString cmd = commandEdit->text().trimmed();
        auto opt = AlarmManager::parse(cmd, labelEdit->text().trimmed());
        if (!opt) {
            QMessageBox::warning(&dlg, tr("Parse error"),
                                 tr("Could not parse command: %1").arg(cmd));
            return;
        }
        whenEdit->setDateTime(opt->triggerUtc.toLocalTime());
        if (!opt->command.isEmpty())
            commandEdit->setText(opt->command);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    // Pick up state changes made while the dialog was open (fired, snoozed, ...)
    const Alarm* latest = m_manager->alarmById(id);
    if (!latest)
        return false; // removed meanwhile
    Alarm updated = *latest;
    const QString newCmd = commandEdit->text().trimmed();
    updated.command = newCmd;
    updated.label = labelEdit->text().trimmed();

    // If command changed, re-parse into trigger; else keep When field
    if (newCmd != a->command && !newCmd.isEmpty()) {
        auto opt = AlarmManager::parse(newCmd, updated.label);
        if (opt) {
            updated.command = opt->command.isEmpty() ? newCmd : opt->command;
            updated.triggerUtc = opt->triggerUtc;
            const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
            while (updated.triggerUtc <= nowUtc)
                updated.triggerUtc = updated.triggerUtc.addDays(1);
        } else {
            QDateTime local = whenEdit->dateTime();
            local.setTimeZone(QTimeZone::systemTimeZone());
            updated.triggerUtc = local.toUTC();
        }
    } else {
        QDateTime local = whenEdit->dateTime();
        local.setTimeZone(QTimeZone::systemTimeZone());
        updated.triggerUtc = local.toUTC();
    }

    updated.acknowledged = doneCheck->isChecked();
    // Re-arm; if the new time is already past, the next tick fires it again.
    updated.triggered = false;

    m_manager->update(updated);
    // Any open notification belongs to the old schedule.
    m_activeTriggered.remove(id);
    if (auto* d = m_dialogs.take(id))
        d->deleteLater();
    return true;
}

void MainWindow::refreshList() {
    // Active first (by time), then DONE at the bottom
    QVector<const Alarm*> ordered;
    for (const auto& a : m_manager->alarms())
        if (!a.acknowledged)
            ordered.append(&a);
    for (const auto& a : m_manager->alarms())
        if (a.acknowledged)
            ordered.append(&a);

    // This runs every second. Only rebuild the rows when the set or order of
    // alarms changed; otherwise update the cells in place so selection,
    // current row and scroll position survive the countdown refresh.
    bool sameRows = m_table->rowCount() == ordered.size();
    for (int row = 0; sameRows && row < ordered.size(); ++row) {
        auto* item = m_table->item(row, kColStatus);
        sameRows = item && item->data(kRoleId).toUuid() == ordered[row]->id;
    }

    QSet<QUuid> selected;
    QUuid currentId;
    if (!sameRows) {
        for (const QModelIndex& idx : m_table->selectionModel()->selectedRows()) {
            if (auto* item = m_table->item(idx.row(), kColStatus))
                selected.insert(item->data(kRoleId).toUuid());
        }
        if (auto* item = m_table->item(m_table->currentRow(), kColStatus))
            currentId = item->data(kRoleId).toUuid();
        m_table->setRowCount(0);
        m_table->setRowCount(ordered.size());
    }

    auto setCell = [this](int row, int col, const QString& text) {
        auto* item = m_table->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if (col == kColStatus)
                item->setTextAlignment(Qt::AlignCenter);
            else if (col == kColRemaining)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(row, col, item);
        }
        if (item->text() != text)
            item->setText(text);
        return item;
    };

    QItemSelection restoreSelection;
    for (int row = 0; row < ordered.size(); ++row) {
        const Alarm& a = *ordered[row];
        const QDateTime local = a.triggerUtc.toLocalTime();

        auto* status = setCell(row, kColStatus, statusText(a));
        status->setData(kRoleId, a.id);
        setCell(row, kColRemaining,
                a.acknowledged ? QStringLiteral("—") : a.remainingString());
        setCell(row, kColWhen, local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")));
        setCell(row, kColCommand, a.command);
        setCell(row, kColLabel, a.label.isEmpty() ? QStringLiteral("—") : a.label);

        const QColor bg = rowBackground(a);
        for (int col = 0; col < m_table->columnCount(); ++col)
            m_table->item(row, col)->setBackground(bg.isValid() ? QBrush(bg) : QBrush());

        if (!sameRows) {
            if (selected.contains(a.id))
                restoreSelection.select(m_table->model()->index(row, 0),
                                        m_table->model()->index(row, m_table->columnCount() - 1));
            if (a.id == currentId)
                m_table->selectionModel()->setCurrentIndex(m_table->model()->index(row, 0),
                                                           QItemSelectionModel::NoUpdate);
        }
    }
    if (!restoreSelection.isEmpty())
        m_table->selectionModel()->select(restoreSelection, QItemSelectionModel::ClearAndSelect);

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

    // Iterate a copy: entries are removed from the set inside the loop.
    const QSet<QUuid> ids = m_activeTriggered;
    for (const QUuid& id : ids) {
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
        // Dialog has WA_DeleteOnClose; only drop our pointer
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
        QApplication::exit(0);
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
