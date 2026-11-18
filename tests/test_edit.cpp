// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmEdit.h"
#include "AlarmManager.h"

#include <QTest>

class TestEdit : public QObject {
    Q_OBJECT
private slots:
    void unchanged_keeps_schedule_and_snooze();
    void label_and_style_only();
    void new_expression_reschedules();
    void new_expression_moves_note_and_modifiers();
    void invalid_expression_is_error();
    void empty_expression_is_error();
    void absolute_past_date_is_not_rolled();
    void reopen_done_reevaluates();
    void reopen_done_with_new_expression();
    void state_changes();
    void recurring_cannot_be_done();
    void switch_to_recurring();
    void normalize_is_idempotent_for_stored_commands();
};

static AlarmEdit editOf(const Alarm& a) {
    AlarmEdit e;
    e.expression = a.command;
    e.label = a.label;
    e.state = alarmState(a);
    e.style = a.notificationStyle;
    return e;
}

static Alarm make(const char* input) {
    auto a = AlarmManager::parse(QString::fromUtf8(input));
    Q_ASSERT(a);
    return *a;
}

void TestEdit::unchanged_keeps_schedule_and_snooze() {
    Alarm a = make("in 5m tea");
    a.triggerUtc = a.triggerUtc.addSecs(300);
    a.snoozed = true;
    a.triggered = true;
    const auto out = applyAlarmEdit(a, a.command, editOf(a));
    QVERIFY(out.alarm);
    QVERIFY(!out.rescheduled);
    QCOMPARE(*out.alarm, a);
}

void TestEdit::label_and_style_only() {
    const Alarm a = make("at 15:10 meeting");
    AlarmEdit e = editOf(a);
    e.label = QStringLiteral("  standup ");
    e.style = NotificationStyle::Silent;
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QVERIFY(!out.rescheduled);
    QCOMPARE(out.alarm->label, QStringLiteral("standup"));
    QCOMPARE(out.alarm->notificationStyle, NotificationStyle::Silent);
    QCOMPARE(out.alarm->triggerUtc, a.triggerUtc);
}

void TestEdit::new_expression_reschedules() {
    Alarm a = make("in 5m");
    a.snoozed = true;
    a.triggered = true;
    a.missed = true;
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("in 2h");
    const QDateTime before = QDateTime::currentDateTimeUtc();
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QVERIFY(out.rescheduled);
    QCOMPARE(out.alarm->command, QStringLiteral("in 2h"));
    QCOMPARE(out.alarm->id, a.id);
    QCOMPARE(out.alarm->scheduledUtc, out.alarm->triggerUtc);
    QVERIFY(qAbs(before.secsTo(out.alarm->triggerUtc) - 7200) <= 2);
    QVERIFY(!out.alarm->snoozed);
    QVERIFY(!out.alarm->triggered);
    QVERIFY(!out.alarm->missed);
}

void TestEdit::new_expression_moves_note_and_modifiers() {
    const Alarm a = make("in 5m");
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("silent disabled in 10m stretch");
    QVERIFY(e.normalize());
    QCOMPARE(e.expression, QStringLiteral("in 10m"));
    QCOMPARE(e.label, QStringLiteral("stretch"));
    QCOMPARE(e.style, NotificationStyle::Silent);
    QCOMPARE(e.state, AlarmState::Disabled);

    // applyAlarmEdit normalizes on its own as well
    e = editOf(a);
    e.expression = QStringLiteral("max at 15:10 call");
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QCOMPARE(out.alarm->command, QStringLiteral("at 15:10"));
    QCOMPARE(out.alarm->label, QStringLiteral("call"));
    QCOMPARE(out.alarm->notificationStyle, NotificationStyle::Fullscreen);
}

void TestEdit::invalid_expression_is_error() {
    const Alarm a = make("in 5m");
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("whenever");
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(!out.alarm);
    QVERIFY(!out.error.isEmpty());
    QVERIFY(!e.normalize());
    QCOMPARE(e.expression, QStringLiteral("whenever"));
}

void TestEdit::empty_expression_is_error() {
    const Alarm a = make("in 5m");
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("  ");
    QVERIFY(!applyAlarmEdit(a, a.command, e).alarm);
}

void TestEdit::absolute_past_date_is_not_rolled() {
    const Alarm a = make("in 5m");
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("2020-01-02 09:30");
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QCOMPARE(out.alarm->scheduledUtc.toLocalTime(),
             QDateTime(QDate(2020, 1, 2), QTime(9, 30)));
}

void TestEdit::reopen_done_reevaluates() {
    Alarm a = make("in 5m");
    a.triggerUtc = a.scheduledUtc = QDateTime::currentDateTimeUtc().addSecs(-3600);
    a.acknowledged = true;
    AlarmEdit e = editOf(a);
    e.state = AlarmState::Active;
    const QDateTime before = QDateTime::currentDateTimeUtc();
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QVERIFY(out.rescheduled);
    QVERIFY(!out.alarm->acknowledged);
    QVERIFY(qAbs(before.secsTo(out.alarm->triggerUtc) - 300) <= 2);

    // Reopening as paused also re-evaluates
    e.state = AlarmState::Disabled;
    const auto paused = applyAlarmEdit(a, a.command, e);
    QVERIFY(paused.alarm);
    QVERIFY(paused.rescheduled);
    QVERIFY(paused.alarm->disabled);
    QVERIFY(!paused.alarm->acknowledged);
}

void TestEdit::reopen_done_with_new_expression() {
    Alarm a = make("in 5m");
    a.acknowledged = true;
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("disabled in 1h");
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QVERIFY(out.alarm->disabled);
    QVERIFY(!out.alarm->acknowledged);
}

void TestEdit::state_changes() {
    Alarm a = make("in 5m");
    a.triggered = true;
    AlarmEdit e = editOf(a);

    e.state = AlarmState::Disabled;
    auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm && out.alarm->disabled && !out.alarm->triggered);
    QCOMPARE(out.alarm->triggerUtc, a.triggerUtc); // pausing keeps the time

    e.state = AlarmState::Done;
    out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm && out.alarm->acknowledged && !out.alarm->disabled);
    QVERIFY(!out.rescheduled);

    Alarm paused = a;
    paused.disabled = true;
    paused.missed = true;
    e = editOf(paused);
    e.state = AlarmState::Active;
    out = applyAlarmEdit(paused, paused.command, e);
    QVERIFY(out.alarm && !out.alarm->disabled && !out.alarm->missed);
    QCOMPARE(out.alarm->triggerUtc, paused.triggerUtc);
}

void TestEdit::recurring_cannot_be_done() {
    const Alarm a = make("every monday at 18:00");
    AlarmEdit e = editOf(a);
    e.state = AlarmState::Done;
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(!out.alarm);
    QVERIFY(!out.error.isEmpty());

    // Done one-shot turned into a repeating rule while staying Done
    Alarm done = make("in 5m");
    done.acknowledged = true;
    e = editOf(done);
    e.expression = QStringLiteral("every 5m");
    QVERIFY(!applyAlarmEdit(done, done.command, e).alarm);
}

void TestEdit::switch_to_recurring() {
    const Alarm a = make("in 5m");
    AlarmEdit e = editOf(a);
    e.expression = QStringLiteral("every weekday at 9:00");
    const auto out = applyAlarmEdit(a, a.command, e);
    QVERIFY(out.alarm);
    QCOMPARE(out.alarm->recurrence.kind, Recurrence::Kind::Weekly);
    QCOMPARE(out.alarm->recurrence.weekdays, Recurrence::kWeekdays);

    // ... and back to a one-shot drops the rule
    AlarmEdit back = editOf(*out.alarm);
    back.expression = QStringLiteral("tomorrow 9:00");
    const auto oneShot = applyAlarmEdit(*out.alarm, out.alarm->command, back);
    QVERIFY(oneShot.alarm);
    QVERIFY(!oneShot.alarm->recurrence.isRecurring());
}

void TestEdit::normalize_is_idempotent_for_stored_commands() {
    for (const char* input : {"in 5m tea", "at 15:10 call", "tomorrow 9:00 x",
                              "next friday 5pm", "every 30m water",
                              "every monday at 18:00 laundry", "2026-12-24 18:00 xmas",
                              "in 1h30", "in an hour and a half"}) {
        const Alarm a = make(input);
        AlarmEdit e;
        e.expression = a.command;
        QVERIFY2(e.normalize(), input);
        QCOMPARE(e.expression, a.command);
        QVERIFY2(e.label.isEmpty(), input);
    }
}

QTEST_GUILESS_MAIN(TestEdit)
#include "test_edit.moc"
