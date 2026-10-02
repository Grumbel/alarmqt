#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>
#include <QIcon>
#include <QSet>
#include <QTimeZone>

MainWindow::MainWindow(AlarmManager* manager, QWidget* parent)
    : QMainWindow(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("AlarmQt"));
    setMinimumSize(420, 320);
    resize(480, 400);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);

    // Input row
    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText(tr("in 5m  ·  at 15:10  ·  at 2026-10-03 09:00"));
    m_input->setClearButtonEnabled(true);
    auto* addBtn = new QPushButton(tr("Add"));
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(addBtn);
    layout->addLayout(inputRow);

    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::addFromInput);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addFromInput);

    // List
    m_list = new QListWidget;
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_list, 1);

    // Status
    m_status = new QLabel;
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    // Shortcuts
    new QShortcut(QKeySequence::New, this, [this]() { m_input->setFocus(); m_input->selectAll(); });
    new QShortcut(QKeySequence::Delete, this, &MainWindow::removeSelected);
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this, &MainWindow::removeSelected);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this]() { hide(); });

    connect(m_manager, &AlarmManager::alarmsChanged, this, &MainWindow::refreshList);
    connect(m_manager, &AlarmManager::alarmTriggered, this, &MainWindow::onAlarmTriggered);
    connect(m_manager, &AlarmManager::alarmAcknowledged, this, [this](const QUuid& id) {
        if (auto* d = m_dialogs.take(id)) {
            d->deleteLater();
        }
    });

    createTray();
    refreshList();

    // Focus input on start
    m_input->setFocus();
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
    // Close → hide to tray
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
                                "  at 2026-10-03 09:00").arg(text));
        return;
    }
    m_manager->add(*opt);
    m_input->clear();
}

void MainWindow::removeSelected() {
    const auto items = m_list->selectedItems();
    for (QListWidgetItem* item : items) {
        const QUuid id = item->data(Qt::UserRole).toUuid();
        m_manager->remove(id);
        if (auto* d = m_dialogs.take(id))
            d->deleteLater();
    }
}

void MainWindow::refreshList() {
    // Preserve selection
    QSet<QUuid> selected;
    for (QListWidgetItem* item : m_list->selectedItems())
        selected.insert(item->data(Qt::UserRole).toUuid());

    m_list->clear();
    const auto nowLocal = QDateTime::currentDateTime();

    for (const auto& a : m_manager->alarms()) {
        if (a.acknowledged)
            continue; // hide finished ones (they stay in storage for a while)

        const QDateTime local = a.triggerUtc.toLocalTime();
        QString text = QStringLiteral("%1    →  %2    (%3)")
                           .arg(a.remainingString(),
                                local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")),
                                a.label);

        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, a.id);
        if (a.triggered)
            item->setForeground(QColor(QStringLiteral("#fc8181")));
        else if (a.remainingMs() < 60'000)
            item->setForeground(QColor(QStringLiteral("#f6e05e")));
        m_list->addItem(item);

        if (selected.contains(a.id))
            item->setSelected(true);
    }

    updateTray();

    if (auto next = m_manager->nextAlarm()) {
        const QDateTime local = next->triggerUtc.toLocalTime();
        m_status->setText(tr("Next: %1  (%2)  —  %3")
                              .arg(next->remainingString(),
                                   local.toString(QStringLiteral("HH:mm:ss")),
                                   next->label));
    } else {
        m_status->setText(tr("No active alarms"));
    }
}

void MainWindow::updateTray() {
    if (auto next = m_manager->nextAlarm()) {
        m_tray->setToolTip(tr("AlarmQt – next in %1\n%2")
                               .arg(next->remainingString(), next->label));
        // Optional: change icon color when soon, but SVG is static
    } else {
        m_tray->setToolTip(tr("AlarmQt – no alarms"));
    }
}

void MainWindow::onAlarmTriggered(const Alarm& a) {
    showNotification(a);

    // Also a tray balloon
    m_tray->showMessage(tr("Alarm"), a.label,
                        QSystemTrayIcon::Warning, 10000);
}

void MainWindow::showNotification(const Alarm& a) {
    // If already showing, bring to front
    if (auto* existing = m_dialogs.value(a.id)) {
        existing->raise();
        existing->activateWindow();
        return;
    }

    auto* dlg = new NotificationDialog(a, this);
    m_dialogs.insert(a.id, dlg);

    connect(dlg, &NotificationDialog::acknowledged, this, [this](const QUuid& id) {
        m_manager->acknowledge(id);
        m_dialogs.remove(id);
    });
    connect(dlg, &NotificationDialog::snoozed, this, [this](const QUuid& id, int mins) {
        m_manager->snooze(id, mins);
        m_dialogs.remove(id);
    });
    connect(dlg, &QObject::destroyed, this, [this, id = a.id]() {
        m_dialogs.remove(id);
    });
}

void MainWindow::handleExternalCommand(const QString& cmd) {
    if (cmd == QLatin1String("--raise") || cmd.isEmpty()) {
        raiseAndActivate();
        return;
    }
    if (cmd == QLatin1String("--quit")) {
        qApp->quit();
        return;
    }
    if (cmd == QLatin1String("--list")) {
        // secondary already printed; nothing to do
        return;
    }

    // Treat as alarm string
    auto opt = AlarmManager::parse(cmd);
    if (opt) {
        m_manager->add(*opt);
        raiseAndActivate();
        m_tray->showMessage(tr("Alarm added"), opt->label, QSystemTrayIcon::Information, 3000);
    } else {
        m_tray->showMessage(tr("Parse error"), tr("Could not parse: %1").arg(cmd),
                            QSystemTrayIcon::Warning, 5000);
    }
}
