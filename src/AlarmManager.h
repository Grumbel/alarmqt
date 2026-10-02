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

    QVector<Alarm> alarms() const { return m_alarms; }
    const Alarm* alarmById(const QUuid& id) const;
    Alarm* alarmById(const QUuid& id);

    // Parsing helpers – return nullopt on failure
    static std::optional<Alarm> parse(const QString& input, const QString& label = {});

    void add(const Alarm& a);
    void update(const Alarm& a); // replace existing by id
    void remove(const QUuid& id);
    void acknowledge(const QUuid& id);
    void snooze(const QUuid& id, int minutes = -1); // -1 → use alarm's default

    void load();
    void save() const;

    // Next non-acknowledged alarm (soonest)
    std::optional<Alarm> nextAlarm() const;

signals:
    void alarmsChanged();
    void alarmTriggered(const Alarm& a);
    void alarmAcknowledged(const QUuid& id);

private slots:
    void tick();

private:
    QVector<Alarm> m_alarms;
    QTimer m_timer;
    QString storagePath() const;
};
