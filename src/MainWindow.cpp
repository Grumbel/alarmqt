// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QResizeEvent>
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
#include <QFontMetrics>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTextBrowser>

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
    if (a.missed)
        return QColor(0x9b, 0x2c, 0x2c, 0x55); // deep red MISSED
    if (a.triggered)
        return QColor(0xfc, 0x81, 0x81, 0x55); // soft red DUE
    if (a.snoozed)
        return QColor(0x63, 0xb3, 0xed, 0x40); // soft blue SNOOZED
    if (a.remainingMs() < 60'000)
        return QColor(0xf6, 0xe0, 0x5e, 0x55); // soft yellow
    return QColor();
}

} // namespace

// Big clock text that shrinks its font to fit the width it is given.
// Horizontally it takes whatever space the layout offers (it never forces
// the window wider); its height stays that of the largest font so the rest
// of the window does not jump while resizing.
class ClockLabel : public QLabel {
public:
    ClockLabel(int maxPointSize, int minPointSize, QWidget* parent = nullptr)
        : QLabel(parent)
        , m_maxPointSize(maxPointSize)
        , m_minPointSize(minPointSize)
    {
        setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setContentsMargins(0, 4, 0, 4);
        QFont f = font();
        f.setBold(true);
        f.setPointSize(m_maxPointSize);
        setFont(f);
        setFixedHeight(QFontMetrics(f).height() + 8);
    }

    void setClockText(const QString& text) {
        setText(text);
        fitFont();
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        fitFont();
    }

private:
    void fitFont() {
        const int available = contentsRect().width();
        if (available <= 0 || text().isEmpty())
            return;
        QFont f = font();
        int size = m_maxPointSize;
        f.setPointSize(size);
        while (size > m_minPointSize && QFontMetrics(f).horizontalAdvance(text()) > available)
            f.setPointSize(--size);
        if (font().pointSize() != size)
            setFont(f);
    }

    int m_maxPointSize;
    int m_minPointSize;
};

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

    m_clock = new ClockLabel(font().pointSize() + 14, std::max(9, font().pointSize()));

    clockRow->addWidget(m_clockIcon, 0, Qt::AlignVCenter);
    clockRow->addWidget(m_clock, 1, Qt::AlignVCenter);
    layout->addLayout(clockRow);
    updateClock();

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText(
        tr("in 10m stretch  ·  at 15:10 team call  ·  every monday at 18:00 laundry"));
    m_input->setClearButtonEnabled(true);
    auto* helpBtn = new QPushButton(tr("?"));
    helpBtn->setToolTip(tr("Alarm time syntax (F1)"));
    helpBtn->setFixedWidth(helpBtn->sizeHint().height() + 8);
    helpBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(helpBtn);
    layout->addLayout(inputRow);

    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::addFromInput);
    connect(helpBtn, &QPushButton::clicked, this, &MainWindow::showSyntaxHelp);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels(
        {tr("Status"), tr("Remaining"), tr("When"), tr("Command"), tr("Label")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    auto* header = m_table->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionsMovable(true); // drag headers to reorder columns
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(kColLabel, QHeaderView::Stretch);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::StrongFocus);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MainWindow::onRowDoubleClicked);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &MainWindow::onTableContextMenu);

    // Footer: QStatusBar with size grip; permanent label for next-alarm line.
    m_status = new QLabel;
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setMinimumWidth(120);
    statusBar()->addPermanentWidget(m_status, 1);
    statusBar()->setSizeGripEnabled(true);

    new QShortcut(QKeySequence::New, this, [this]() {
        m_input->setFocus();
        m_input->selectAll();
    });
    new QShortcut(QKeySequence::Delete, this, [this]() { removeSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this, [this]() { removeSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this, [this]() { editSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this, [this]() { restartSelected(); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this, [this]() { skipSelected(); });
    new QShortcut(QKeySequence::HelpContents, this, [this]() { showSyntaxHelp(); });
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

    m_rowBlinkTimer.setInterval(250);
    connect(&m_rowBlinkTimer, &QTimer::timeout, this, [this]() {
        if (m_activeTriggered.isEmpty())
            return;
        m_rowBlinkOn = !m_rowBlinkOn;
        refreshList();
    });
    m_rowBlinkTimer.start();

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
    m_clock->setClockText(now.toString(QStringLiteral("dddd  yyyy-MM-dd  HH:mm:ss")));
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
                                "  in 10m stretch\n"
                                "  in 5m, water plants\n"
                                "  at 15:10 team call\n"
                                "  in 2h30m\n"
                                "  at 2026-10-03 09:00\n"
                                "  next monday 5:50pm\n"
                                "  tomorrow 9:00\n"
                                "  in 2 weeks\n"
                                "  every 30m drink water\n"
                                "  every monday at 18:00 laundry\n"
                                "  every weekday at 9:00 standup\n\n"
                                "Press Help or F1 for the full syntax.")
                                 .arg(text));
        return;
    }
    m_manager->add(*opt);
    m_input->clear();
}

void MainWindow::showSyntaxHelp() {
    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Alarm time syntax"));
    dlg->resize(560, 520);

    auto* layout = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(false);
    browser->setHtml(tr(
        "<h2>Alarm time syntax</h2>"
        "<p>Type a time expression in the input line, optionally followed by a "
        "note. The time part becomes the <b>Command</b>; trailing text becomes "
        "the <b>Label</b>.</p>"

        "<h3>Relative</h3>"
        "<p>Count from now. The word <code>in</code> is optional.</p>"
        "<ul>"
        "<li><code>in 5m</code> &nbsp; <code>5m</code> &nbsp; <code>30s</code></li>"
        "<li><code>in 2h30m</code> &nbsp; <code>in 1d</code></li>"
        "<li><code>in 10 mins</code> &nbsp; <code>in 2 hours</code> &nbsp; "
        "<code>in 1 hour 15 minutes</code></li>"
        "<li><code>in 2 weeks</code> &nbsp; <code>in 1 month</code></li>"
        "</ul>"
        "<p>Units: <code>s</code>/<code>sec</code>/<code>secs</code>/<code>second</code>/<code>seconds</code>, "
        "<code>m</code>/<code>min</code>/<code>mins</code>/<code>minute</code>/<code>minutes</code>, "
        "<code>h</code>/<code>hr</code>/<code>hrs</code>/<code>hour</code>/<code>hours</code>, "
        "<code>d</code>/<code>day</code>/<code>days</code>.</p>"

        "<h3>Absolute</h3>"
        "<p>A clock time today (or tomorrow if that time has already passed), "
        "or a full date-time. The word <code>at</code> is optional.</p>"
        "<ul>"
        "<li><code>at 15:10</code> &nbsp; <code>15:10</code></li>"
        "<li><code>6:00pm</code> &nbsp; <code>6am</code> &nbsp; <code>6:00 p.m.</code></li>"
        "<li><code>at 2026-10-03 09:00</code> &nbsp; <code>at 2026-10-06 5:00pm</code></li>"
        "<li><code>today 17:00</code> &nbsp; <code>tomorrow 9:00</code> &nbsp; <code>tomorrow noon</code></li>"
        "<li><code>next monday 5:50pm</code> &nbsp; <code>monday 9:00</code></li>"
        "<li><code>noon</code> &nbsp; <code>midnight</code></li>"
        "</ul>"

        "<h3>Timezone on absolute times</h3>"
        "<p>A zone may be <b>glued</b> to the time (no space). A space starts the label.</p>"
        "<ul>"
        "<li><code>at 15:10CEST</code> &nbsp; <code>at 12:00Z</code> &nbsp; "
        "<code>at 15:10+02:00</code></li>"
        "<li><code>at 15:10CEST ship it</code> — zone CEST, label “ship it”</li>"
        "<li><code>at 15:10 CEST</code> — system zone, label “CEST”</li>"
        "</ul>"

        "<h3>Repeating</h3>"
        "<p><code>each</code> is accepted in place of <code>every</code>.</p>"
        "<p><b>Interval</b> — fires again after the interval, counted from "
        "acknowledgement (an ignored alarm stays due):</p>"
        "<ul>"
        "<li><code>every 5m</code> &nbsp; <code>every 1h30m</code> &nbsp; "
        "<code>every hour</code></li>"
        "</ul>"
        "<p><b>Weekly</b> — local wall-clock time; stays fixed across DST. "
        "Days may be full or short names, plural, joined with commas, "
        "<code>and</code>, <code>&amp;</code>, or <code>/</code>:</p>"
        "<ul>"
        "<li><code>every monday at 18:00</code></li>"
        "<li><code>every mon, thu 6pm</code></li>"
        "<li><code>every mon and fri at 9:00</code></li>"
        "</ul>"
        "<p><b>Daily / weekdays / weekends</b>:</p>"
        "<ul>"
        "<li><code>daily at 7:30</code> &nbsp; <code>every day at 7:30</code></li>"
        "<li><code>every weekday at 9:00</code></li>"
        "<li><code>weekends 10am</code> &nbsp; <code>every weekend at 10:00</code></li>"
        "</ul>"
        "<p>Acknowledging a repeating alarm schedules the next occurrence "
        "instead of marking it DONE. <b>Skip next</b> (context menu, Ctrl+K) "
        "drops the upcoming occurrence. Remove the alarm to stop it.</p>"

        "<h3>Notes (labels)</h3>"
        "<p>Text after the time expression becomes the label:</p>"
        "<ul>"
        "<li><code>in 10m stretch</code></li>"
        "<li><code>in 5m, water plants</code></li>"
        "<li><code>at 15:10 team call</code></li>"
        "<li><code>in 5m (laundry)</code> &nbsp; <code>in 10m \"pick up kids\"</code></li>"
        "</ul>"

        "<h3>Examples</h3>"
        "<ul>"
        "<li><code>in 5m</code></li>"
        "<li><code>in 10m stretch</code></li>"
        "<li><code>in 5m, water plants</code></li>"
        "<li><code>at 15:10 team call</code></li>"
        "<li><code>at 15:10CEST ship it</code></li>"
        "<li><code>in 2h30m</code></li>"
        "<li><code>at 2026-10-03 09:00</code></li>"
        "<li><code>next monday 5:50pm</code></li>"
        "<li><code>tomorrow 9:00</code></li>"
        "<li><code>in 2 weeks</code></li>"
        "<li><code>every 30m drink water</code></li>"
        "<li><code>every monday at 18:00 laundry</code></li>"
        "<li><code>every weekday at 9:00 standup</code></li>"
        "<li><code>daily at 7:30</code></li>"
        "</ul>"
    ));
    layout->addWidget(browser);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(buttons);

    dlg->exec();
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

void MainWindow::skipSelected() {
    QList<QUuid> ids;
    for (const QModelIndex& idx : m_table->selectionModel()->selectedRows()) {
        auto* item = m_table->item(idx.row(), kColStatus);
        if (!item)
            continue;
        const QUuid id = item->data(kRoleId).toUuid();
        if (const Alarm* a = m_manager->alarmById(id); a && a->recurrence.isRecurring())
            ids.append(id);
    }
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Skip next"),
                                 tr("Select a repeating alarm to skip its next occurrence."));
        return;
    }
    // skipNext() closes open notifications via alarmAcknowledged.
    for (const QUuid& id : ids)
        m_manager->skipNext(id);
}

void MainWindow::onTableContextMenu(const QPoint& pos) {
    const QModelIndex index = m_table->indexAt(pos);
    if (index.isValid())
        m_table->selectRow(index.row());

    QMenu menu(this);
    menu.addAction(tr("Edit…"), this, &MainWindow::editSelected);
    menu.addAction(tr("Restart"), this, &MainWindow::restartSelected);
    bool anyRecurring = false;
    for (const QModelIndex& idx : m_table->selectionModel()->selectedRows()) {
        if (auto* item = m_table->item(idx.row(), kColStatus))
            if (const Alarm* a = m_manager->alarmById(item->data(kRoleId).toUuid()))
                anyRecurring |= a->recurrence.isRecurring();
    }
    menu.addAction(tr("Skip next"), this, &MainWindow::skipSelected)->setEnabled(anyRecurring);
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
    const QDateTime editWhen = a->scheduledUtc.isValid() ? a->scheduledUtc : a->triggerUtc;
    auto* whenEdit = new QDateTimeEdit(editWhen.toLocalTime());
    whenEdit->setCalendarPopup(true);
    whenEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    whenEdit->setTimeZone(QTimeZone::systemTimeZone());

    auto* doneCheck = new QCheckBox(tr("Done (acknowledged)"));
    doneCheck->setChecked(a->acknowledged);
    if (a->recurrence.isRecurring()) {
        // Repeating alarms never become DONE; use "Skip next" instead.
        doneCheck->setEnabled(false);
        doneCheck->setToolTip(tr("Repeating alarm (%1); use Skip next or Remove")
                                  .arg(a->recurrence.describe()));
    }

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
            updated.recurrence = opt->recurrence;
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

    // An unparsable new command keeps the old rule rather than silently
    // dropping it; a repeating alarm is never DONE.
    updated.acknowledged = doneCheck->isChecked() && !updated.recurrence.isRecurring();
    // Re-arm; if the new time is already past, the next tick fires it again.
    updated.triggered = false;
    updated.scheduledUtc = updated.triggerUtc;
    updated.snoozed = false;
    updated.missed = false;

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

        const bool recurring = a.recurrence.isRecurring();
        auto* status = setCell(row, kColStatus,
                               recurring ? a.statusText() + QStringLiteral(" ↻") : a.statusText());
        status->setData(kRoleId, a.id);

        QString remaining;
        if (a.acknowledged) {
            remaining = QStringLiteral("—");
        } else if (a.snoozed) {
            remaining = tr("snooze %1").arg(a.remainingString());
        } else {
            remaining = a.remainingString();
        }
        setCell(row, kColRemaining, remaining);

        // "When" is the real scheduled time; snooze only moves triggerUtc.
        const QDateTime whenSrc = a.scheduledUtc.isValid() ? a.scheduledUtc : a.triggerUtc;
        setCell(row, kColWhen, whenSrc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        auto* command = setCell(row, kColCommand, a.command);
        const QString repeatTip = recurring ? tr("Repeats: %1").arg(a.recurrence.describe())
                                            : QString();
        status->setToolTip(repeatTip);
        command->setToolTip(repeatTip);
        setCell(row, kColLabel, a.label.isEmpty() ? QStringLiteral("—") : a.label);

        // Row blink for open notifications overrides static status colour.
        QColor bg;
        if (m_activeTriggered.contains(a.id)) {
            bg = m_rowBlinkOn ? QColor(0xe5, 0x3e, 0x3e, 0x90)
                              : QColor(0x00, 0x00, 0x00, 0x70);
        } else {
            bg = rowBackground(a);
        }
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
        const QDateTime whenSrc = next->scheduledUtc.isValid() ? next->scheduledUtc
                                                               : next->triggerUtc;
        const QDateTime local = whenSrc.toLocalTime();
        QString remaining = next->snoozed
                                ? tr("snooze %1").arg(next->remainingString())
                                : next->remainingString();
        m_status->setText(tr("Next: %1  (%2)  —  %3")
                              .arg(remaining,
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

