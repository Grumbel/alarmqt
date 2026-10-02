// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmManager.h"
#include "Recurrence.h"

#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

class TestRecurrence : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    void weekly_same_day_and_next_week();
    void weekly_multiple_days();
    void weekly_keeps_wall_clock_across_dst();
    void weekly_spring_forward_gap();
    void interval_next();
    void invalid_rules();
    void describe();
    void json_roundtrip();

    void parse_weekly();
    void parse_weekly_variants();
    void parse_interval();
    void parse_rejects();
    void parse_one_shot_has_no_recurrence();

    void manager_ack_rearms_interval();
    void manager_ack_weekly_skips_missed();
    void manager_skip_next();
    void manager_restart_keeps_recurrence();

private:
    QTemporaryDir m_stateDir;
};

static const QTimeZone& berlin() {
    static const QTimeZone tz("Europe/Berlin");
    return tz;
}

static QDateTime berlinTime(int y, int mo, int d, int h, int mi) {
    return QDateTime(QDate(y, mo, d), QTime(h, mi), berlin()).toUTC();
}

static Recurrence weekly(quint8 mask, QTime t) {
    Recurrence r;
    r.kind = Recurrence::Kind::Weekly;
    r.weekdays = mask;
    r.time = t;
    return r;
}

static quint8 dayBit(Qt::DayOfWeek d) {
    return static_cast<quint8>(1 << (d - 1));
}

void TestRecurrence::initTestCase() {
    QVERIFY(berlin().isValid());
    // Keep AlarmManager away from the user's real alarm file.
    QVERIFY(m_stateDir.isValid());
    qputenv("XDG_STATE_HOME", m_stateDir.path().toUtf8());
}

void TestRecurrence::weekly_same_day_and_next_week() {
    const Recurrence r = weekly(dayBit(Qt::Monday), QTime(18, 0));
    // 2026-10-05 is a Monday.
    QCOMPARE(r.nextAfter(berlinTime(2026, 10, 5, 10, 0), berlin()),
             berlinTime(2026, 10, 5, 18, 0));
    // Strictly after: exactly 18:00 moves to next week.
    QCOMPARE(r.nextAfter(berlinTime(2026, 10, 5, 18, 0), berlin()),
             berlinTime(2026, 10, 12, 18, 0));
    // From a Tuesday.
    QCOMPARE(r.nextAfter(berlinTime(2026, 10, 6, 9, 0), berlin()),
             berlinTime(2026, 10, 12, 18, 0));
}

void TestRecurrence::weekly_multiple_days() {
    const Recurrence r = weekly(dayBit(Qt::Monday) | dayBit(Qt::Thursday), QTime(6, 30));
    QCOMPARE(r.nextAfter(berlinTime(2026, 10, 5, 7, 0), berlin()),
             berlinTime(2026, 10, 8, 6, 30));
    QCOMPARE(r.nextAfter(berlinTime(2026, 10, 8, 7, 0), berlin()),
             berlinTime(2026, 10, 12, 6, 30));
}

void TestRecurrence::weekly_keeps_wall_clock_across_dst() {
    // DST ends in Berlin on Sunday 2026-10-25.
    const Recurrence mon = weekly(dayBit(Qt::Monday), QTime(18, 0));
    const QDateTime next = mon.nextAfter(berlinTime(2026, 10, 19, 18, 0), berlin());
    QCOMPARE(next.toTimeZone(berlin()).time(), QTime(18, 0));
    QCOMPARE(next, QDateTime(QDate(2026, 10, 26), QTime(17, 0), QTimeZone::utc()));

    // DST starts on Sunday 2026-03-29.
    const Recurrence daily = weekly(Recurrence::kEveryDay, QTime(7, 30));
    const QDateTime spring = daily.nextAfter(berlinTime(2026, 3, 28, 7, 30), berlin());
    QCOMPARE(spring, QDateTime(QDate(2026, 3, 29), QTime(5, 30), QTimeZone::utc()));
}

void TestRecurrence::weekly_spring_forward_gap() {
    // 02:30 does not exist on 2026-03-29 in Berlin; it must still fire that
    // day (moved past the gap) and be back to 02:30 the day after.
    const Recurrence r = weekly(Recurrence::kEveryDay, QTime(2, 30));
    const QDateTime gapDay = r.nextAfter(berlinTime(2026, 3, 28, 12, 0), berlin());
    QVERIFY(gapDay.isValid());
    QCOMPARE(gapDay.toTimeZone(berlin()).date(), QDate(2026, 3, 29));
    const QDateTime after = r.nextAfter(gapDay, berlin());
    QCOMPARE(after.toTimeZone(berlin()).date(), QDate(2026, 3, 30));
    QCOMPARE(after.toTimeZone(berlin()).time(), QTime(2, 30));
}

void TestRecurrence::interval_next() {
    Recurrence r;
    r.kind = Recurrence::Kind::Interval;
    r.intervalSecs = 300;
    const QDateTime t = berlinTime(2026, 10, 5, 10, 0);
    QCOMPARE(r.nextAfter(t, berlin()), t.addSecs(300));
}

void TestRecurrence::invalid_rules() {
    QVERIFY(!Recurrence().nextAfter(QDateTime::currentDateTimeUtc()).isValid());
    QVERIFY(!weekly(0, QTime(8, 0)).nextAfter(QDateTime::currentDateTimeUtc()).isValid());
    QVERIFY(!weekly(Recurrence::kEveryDay, QTime()).nextAfter(QDateTime::currentDateTimeUtc()).isValid());
    Recurrence r;
    r.kind = Recurrence::Kind::Interval;
    QVERIFY(!r.nextAfter(QDateTime::currentDateTimeUtc()).isValid());
}

void TestRecurrence::describe() {
    Recurrence r;
    r.kind = Recurrence::Kind::Interval;
    r.intervalSecs = 5400;
    QCOMPARE(r.describe(), QStringLiteral("every 1h30m"));
    QCOMPARE(weekly(Recurrence::kEveryDay, QTime(7, 30)).describe(), QStringLiteral("daily 07:30"));
    QCOMPARE(weekly(Recurrence::kWeekdays, QTime(9, 0)).describe(), QStringLiteral("weekdays 09:00"));
    QCOMPARE(weekly(dayBit(Qt::Monday) | dayBit(Qt::Thursday), QTime(18, 0)).describe(),
             QStringLiteral("Mon, Thu 18:00"));
}

void TestRecurrence::json_roundtrip() {
    const Recurrence w = weekly(dayBit(Qt::Friday), QTime(17, 45, 10));
    QCOMPARE(Recurrence::fromJson(w.toJson()), w);
    Recurrence i;
    i.kind = Recurrence::Kind::Interval;
    i.intervalSecs = 90;
    QCOMPARE(Recurrence::fromJson(i.toJson()), i);
    QCOMPARE(Recurrence::fromJson(QJsonObject()), Recurrence());

    // Through Alarm, and old files with the unused "repeating" flag.
    Alarm a;
    a.id = QUuid::createUuid();
    a.command = QStringLiteral("every fri at 17:45:10");
    a.triggerUtc = QDateTime::currentDateTimeUtc();
    a.scheduledUtc = a.triggerUtc;
    a.recurrence = w;
    QCOMPARE(Alarm::fromJson(a.toJson()).recurrence, w);
    QJsonObject old = a.toJson();
    old.remove(QStringLiteral("recurrence"));
    old[QStringLiteral("repeating")] = true;
    QVERIFY(!Alarm::fromJson(old).recurrence.isRecurring());
}

void TestRecurrence::parse_weekly() {
    auto a = AlarmManager::parse(QStringLiteral("each monday at 18:00 laundry"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("each monday at 18:00"));
    QCOMPARE(a->label, QStringLiteral("laundry"));
    QCOMPARE(a->recurrence.kind, Recurrence::Kind::Weekly);
    QCOMPARE(a->recurrence.weekdays, dayBit(Qt::Monday));
    QCOMPARE(a->recurrence.time, QTime(18, 0));

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(a->triggerUtc > now);
    QVERIFY(a->triggerUtc <= now.addDays(7));
    QCOMPARE(a->scheduledUtc, a->triggerUtc);
    const QDateTime local = a->triggerUtc.toLocalTime();
    QCOMPARE(local.date().dayOfWeek(), int(Qt::Monday));
    QCOMPARE(local.time(), QTime(18, 0));
}

void TestRecurrence::parse_weekly_variants() {
    const struct { const char* input; quint8 mask; QTime time; const char* label; } cases[] = {
        {"every mon, thu 6pm, gym", quint8(dayBit(Qt::Monday) | dayBit(Qt::Thursday)), QTime(18, 0), "gym"},
        {"every Tuesday and Friday at 7:05", quint8(dayBit(Qt::Tuesday) | dayBit(Qt::Friday)), QTime(7, 5), ""},
        {"daily at 7:30 (meds)", Recurrence::kEveryDay, QTime(7, 30), "meds"},
        {"every day at 21:00 \"backup\"", Recurrence::kEveryDay, QTime(21, 0), "backup"},
        {"every weekday at 9:00 standup", Recurrence::kWeekdays, QTime(9, 0), "standup"},
        {"weekends 10am", Recurrence::kWeekend, QTime(10, 0), ""},
        {"every sundays 8:15 pm call mom", dayBit(Qt::Sunday), QTime(20, 15), "call mom"},
    };
    for (const auto& c : cases) {
        auto a = AlarmManager::parse(QString::fromUtf8(c.input));
        QVERIFY2(a.has_value(), c.input);
        QCOMPARE(a->recurrence.kind, Recurrence::Kind::Weekly);
        QCOMPARE(a->recurrence.weekdays, c.mask);
        QCOMPARE(a->recurrence.time, c.time);
        QCOMPARE(a->label, QString::fromUtf8(c.label));
    }
}

void TestRecurrence::parse_interval() {
    auto a = AlarmManager::parse(QStringLiteral("every 5m"));
    QVERIFY(a.has_value());
    QCOMPARE(a->recurrence.kind, Recurrence::Kind::Interval);
    QCOMPARE(a->recurrence.intervalSecs, 300);
    QCOMPARE(a->command, QStringLiteral("every 5m"));
    QVERIFY(a->label.isEmpty());
    const qint64 s = QDateTime::currentDateTimeUtc().secsTo(a->triggerUtc);
    QVERIFY2(s >= 298 && s <= 300, qPrintable(QString::number(s)));

    auto b = AlarmManager::parse(QStringLiteral("each 1h30m drink water"));
    QVERIFY(b.has_value());
    QCOMPARE(b->recurrence.intervalSecs, 5400);
    QCOMPARE(b->label, QStringLiteral("drink water"));

    auto c = AlarmManager::parse(QStringLiteral("every hour, stretch"));
    QVERIFY(c.has_value());
    QCOMPARE(c->recurrence.intervalSecs, 3600);
    QCOMPARE(c->label, QStringLiteral("stretch"));

    auto d = AlarmManager::parse(QStringLiteral("every 10 mins"));
    QVERIFY(d.has_value());
    QCOMPARE(d->recurrence.intervalSecs, 600);
}

void TestRecurrence::parse_rejects() {
    QVERIFY(!AlarmManager::parse(QStringLiteral("every")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("every monday")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("every 2 hamburgers")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("every 0m")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("every funday at 10:00")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("every monday at 25:00")).has_value());
}

void TestRecurrence::parse_one_shot_has_no_recurrence() {
    for (const char* in : {"in 5m", "at 15:10 team call", "in 5m, every day"}) {
        auto a = AlarmManager::parse(QString::fromUtf8(in));
        QVERIFY2(a.has_value(), in);
        QVERIFY2(!a->recurrence.isRecurring(), in);
    }
}

void TestRecurrence::manager_ack_rearms_interval() {
    AlarmManager m;
    auto a = AlarmManager::parse(QStringLiteral("every 5m tea"));
    QVERIFY(a.has_value());
    // Pretend it fired a while ago and was ignored.
    a->triggerUtc = QDateTime::currentDateTimeUtc().addSecs(-1200);
    a->scheduledUtc = a->triggerUtc;
    a->triggered = true;
    m.add(*a);

    m.acknowledge(a->id);
    const Alarm* r = m.alarmById(a->id);
    QVERIFY(r);
    QVERIFY(!r->acknowledged);
    QVERIFY(!r->triggered);
    // Counted from the acknowledgement, not from the missed slot.
    const qint64 s = QDateTime::currentDateTimeUtc().secsTo(r->triggerUtc);
    QVERIFY2(s >= 298 && s <= 300, qPrintable(QString::number(s)));
    QCOMPARE(r->scheduledUtc, r->triggerUtc);
    QCOMPARE(m.clearDone(), 0);
    m.remove(a->id);
}

void TestRecurrence::manager_ack_weekly_skips_missed() {
    AlarmManager m;
    auto a = AlarmManager::parse(QStringLiteral("every monday at 18:00 laundry"));
    QVERIFY(a.has_value());
    // Missed for three weeks: one acknowledgement jumps to the next future slot.
    a->triggerUtc = a->triggerUtc.addDays(-21);
    a->scheduledUtc = a->triggerUtc;
    a->missed = true;
    m.add(*a);

    m.acknowledge(a->id);
    const Alarm* r = m.alarmById(a->id);
    QVERIFY(r);
    QVERIFY(!r->acknowledged && !r->missed);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(r->triggerUtc > now);
    QVERIFY(r->triggerUtc <= now.addDays(7));
    QCOMPARE(r->triggerUtc.toLocalTime().time(), QTime(18, 0));
    m.remove(a->id);
}

void TestRecurrence::manager_skip_next() {
    AlarmManager m;
    auto a = AlarmManager::parse(QStringLiteral("every monday at 18:00"));
    QVERIFY(a.has_value());
    const QDateTime upcoming = a->scheduledUtc;
    m.add(*a);

    QVERIFY(m.skipNext(a->id));
    const Alarm* r = m.alarmById(a->id);
    QVERIFY(r);
    QCOMPARE(r->scheduledUtc.toLocalTime(), upcoming.toLocalTime().addDays(7));

    // Snoozed: skip goes from the planned time, not the snooze time.
    m.snooze(a->id, 10);
    QVERIFY(m.skipNext(a->id));
    QCOMPARE(m.alarmById(a->id)->scheduledUtc.toLocalTime(), upcoming.toLocalTime().addDays(14));
    QVERIFY(!m.alarmById(a->id)->snoozed);
    m.remove(a->id);

    auto once = AlarmManager::parse(QStringLiteral("in 5m"));
    m.add(*once);
    QVERIFY(!m.skipNext(once->id));
    m.remove(once->id);
}

void TestRecurrence::manager_restart_keeps_recurrence() {
    AlarmManager m;
    auto a = AlarmManager::parse(QStringLiteral("every 15m"));
    QVERIFY(a.has_value());
    m.add(*a);
    QVERIFY(m.restart(a->id));
    QCOMPARE(m.alarmById(a->id)->recurrence.intervalSecs, 900);
    m.remove(a->id);
}

QTEST_MAIN(TestRecurrence)
#include "test_recurrence.moc"
