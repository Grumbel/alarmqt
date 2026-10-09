// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Alarm.h"

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QDateTime>

#include <optional>
#include <algorithm>

class AlarmManager : public QObject {
    Q_OBJECT
public:
    explicit AlarmManager(QObject* parent = nullptr);

    const QVector<Alarm>& alarms() const { return m_alarms; }
    const Alarm* alarmById(const QUuid& id) const;
    Alarm* alarmById(const QUuid& id);

    // Parsing helpers – return nullopt on failure
    static std::optional<Alarm> parse(const QString& input, const QString& label = {});

    void add(const Alarm& a);
    void update(const Alarm& a); // replace existing by id
    void remove(const QUuid& id);
    /** One-shot alarms become DONE; recurring ones move to their next occurrence. */
    void acknowledge(const QUuid& id);
    /** Recurring only: drop the upcoming occurrence and schedule the one after. */
    bool skipNext(const QUuid& id);
    void snooze(const QUuid& id, int minutes = -1); // -1 → use alarm's default
    /** Re-arm alarm from its command (or same absolute time next occurrence). */
    bool restart(const QUuid& id);
    /** Permanently remove all acknowledged alarms. Returns count removed. */
    int clearDone();
    /** Pause or resume an alarm. Disabled alarms never fire until re-enabled. */
    bool setDisabled(const QUuid& id, bool disabled);

    void load();
    void save() const;

    /** Replace the entire alarm list (used by undo/redo). Emits alarmsChanged. */
    void replaceAll(const QVector<Alarm>& alarms);

    // Next non-acknowledged alarm (soonest)
    std::optional<Alarm> nextAlarm() const;

signals:
    void alarmsChanged();
    void alarmTriggered(const Alarm& a);
    void alarmAcknowledged(const QUuid& id);

private slots:
    void tick();

private:
    void sortAlarms();
    void advanceRecurring(Alarm& a);

    QVector<Alarm> m_alarms;
    QTimer m_timer;
    QString storagePath() const;
};
