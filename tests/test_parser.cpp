// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AlarmManager.h"

#include <QTest>
#include <QTimeZone>

class TestParser : public QObject {
    Q_OBJECT
private slots:
    void relative_basic();
    void relative_combined();
    void relative_optional_in();
    void relative_unit_spellings();
    void absolute_time_only();
    void absolute_full_date();
    void absolute_next_weekday();
    void absolute_today_tomorrow();
    void relative_weeks_months();
    void absolute_date_ampm();
    void absolute_noon_midnight();
    void recurring_monthly();
    void relative_synonyms_combined();
    void absolute_this_next_week();
    void absolute_eod_bare_hour();
    void recurring_yearly_nth_biweekly();
    void absolute_american_ampm();
    void absolute_glued_timezone();
    void notes_trailing_words();
    void notes_comma();
    void notes_parens_and_quotes();
    void explicit_label_overrides_note();
    void invalid_inputs();
    void command_is_time_part_only();
    void leading_modifiers_style();
    void leading_modifiers_disabled();
    void leading_modifiers_combined();
    void relative_clock_style();
    void relative_decimal_amounts();
    void relative_implicit_trailing_unit();
    void relative_and_connector();
    void relative_word_units();
    void relative_from_now();
    void absolute_dot_and_four_digit();
    void tolerant_forms_no_conflicts();
};

static qint64 secsUntil(const Alarm& a) {
    return QDateTime::currentDateTimeUtc().secsTo(a.triggerUtc);
}

void TestParser::relative_basic() {
    auto a = AlarmManager::parse(QStringLiteral("in 5m"));
    QVERIFY(a.has_value());
    QVERIFY(a->label.isEmpty());
    QCOMPARE(a->command, QStringLiteral("in 5m"));
    const qint64 s = secsUntil(*a);
    QVERIFY2(s >= 4 * 60 && s <= 5 * 60 + 2, qPrintable(QString::number(s)));

    auto b = AlarmManager::parse(QStringLiteral("30s"));
    QVERIFY(b.has_value());
    const qint64 sb = secsUntil(*b);
    QVERIFY2(sb >= 28 && sb <= 32, qPrintable(QString::number(sb)));
}

void TestParser::relative_combined() {
    auto a = AlarmManager::parse(QStringLiteral("in 1h30m"));
    QVERIFY(a.has_value());
    const qint64 s = secsUntil(*a);
    QVERIFY2(s >= 89 * 60 && s <= 91 * 60, qPrintable(QString::number(s)));

    auto b = AlarmManager::parse(QStringLiteral("in 1d"));
    QVERIFY(b.has_value());
    const qint64 sb = secsUntil(*b);
    QVERIFY2(sb >= 23 * 3600 && sb <= 25 * 3600, qPrintable(QString::number(sb)));
}

void TestParser::relative_optional_in() {
    auto a = AlarmManager::parse(QStringLiteral("2h"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("2h"));
    const qint64 s = secsUntil(*a);
    QVERIFY2(s >= 7190 && s <= 7210, qPrintable(QString::number(s)));
}

void TestParser::relative_unit_spellings() {
    const struct { const char* input; qint64 secs; } cases[] = {
        {"in 5 mins", 5 * 60},
        {"in 2 hrs", 2 * 3600},
        {"in 1 hour 15 minutes", 75 * 60},
        {"in 30 secs", 30},
        {"in 2 days", 2 * 86400},
    };
    for (const auto& c : cases) {
        auto a = AlarmManager::parse(QString::fromUtf8(c.input));
        QVERIFY2(a.has_value(), c.input);
        QVERIFY2(a->label.isEmpty(), qPrintable(a->label));
        const qint64 s = secsUntil(*a);
        QVERIFY2(s >= c.secs - 2 && s <= c.secs, qPrintable(QString::number(s)));
    }

    auto b = AlarmManager::parse(QStringLiteral("in 5 mins stretch"));
    QVERIFY(b.has_value());
    QCOMPARE(b->label, QStringLiteral("stretch"));
}

void TestParser::absolute_time_only() {
    const QTime t(15, 10);
    auto a = AlarmManager::parse(QStringLiteral("at 15:10"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("at 15:10"));
    const QDateTime local = a->triggerUtc.toLocalTime();
    QCOMPARE(local.time().hour(), 15);
    QCOMPARE(local.time().minute(), 10);
    QVERIFY(a->triggerUtc > QDateTime::currentDateTimeUtc().addSecs(-2));
}

void TestParser::absolute_full_date() {
    auto a = AlarmManager::parse(QStringLiteral("at 2099-06-15 09:30"));
    QVERIFY(a.has_value());
    const QDateTime local = a->triggerUtc.toLocalTime();
    QCOMPARE(local.date(), QDate(2099, 6, 15));
    QCOMPARE(local.time().hour(), 9);
    QCOMPARE(local.time().minute(), 30);

    // Unpadded hour (same as time-only "H:mm"); help examples use padded form.
    auto b = AlarmManager::parse(QStringLiteral("at 2099-10-06 5:00"));
    QVERIFY(b.has_value());
    const QDateTime localB = b->triggerUtc.toLocalTime();
    QCOMPARE(localB.date(), QDate(2099, 10, 6));
    QCOMPARE(localB.time().hour(), 5);
    QCOMPARE(localB.time().minute(), 0);

    auto c = AlarmManager::parse(QStringLiteral("at 2099-10-06T5:00:00"));
    QVERIFY(c.has_value());
    QCOMPARE(c->triggerUtc.toLocalTime().date(), QDate(2099, 10, 6));
    QCOMPARE(c->triggerUtc.toLocalTime().time().hour(), 5);
}

void TestParser::absolute_next_weekday() {
    auto a = AlarmManager::parse(QStringLiteral("next monday 5:50pm"));
    QVERIFY(a.has_value());
    QVERIFY(!a->recurrence.isRecurring());
    QCOMPARE(a->command, QStringLiteral("next monday 5:50pm"));
    QVERIFY(a->label.isEmpty());
    const QDateTime local = a->triggerUtc.toLocalTime();
    QCOMPARE(local.time().hour(), 17);
    QCOMPARE(local.time().minute(), 50);
    QCOMPARE(local.date().dayOfWeek(), 1); // Monday
    QVERIFY(a->triggerUtc > QDateTime::currentDateTimeUtc());

    auto b = AlarmManager::parse(QStringLiteral("next mon at 9:00 laundry"));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("next mon at 9:00"));
    QCOMPARE(b->label, QStringLiteral("laundry"));
    QCOMPARE(b->triggerUtc.toLocalTime().time().hour(), 9);
    QCOMPARE(b->triggerUtc.toLocalTime().date().dayOfWeek(), 1);

    auto c = AlarmManager::parse(QStringLiteral("next friday 18:00"));
    QVERIFY(c.has_value());
    QCOMPARE(c->triggerUtc.toLocalTime().date().dayOfWeek(), 5); // Friday
    QCOMPARE(c->triggerUtc.toLocalTime().time().hour(), 18);

    // Bare weekday (no "next") — same meaning
    auto d = AlarmManager::parse(QStringLiteral("monday 9:00"));
    QVERIFY(d.has_value());
    QVERIFY(!d->recurrence.isRecurring());
    QCOMPARE(d->triggerUtc.toLocalTime().date().dayOfWeek(), 1);
    QCOMPARE(d->triggerUtc.toLocalTime().time().hour(), 9);

    // No time → invalid
    QVERIFY(!AlarmManager::parse(QStringLiteral("next monday")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("next hamburger 5pm")).has_value());
}

void TestParser::absolute_today_tomorrow() {
    auto a = AlarmManager::parse(QStringLiteral("tomorrow 9:00"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("tomorrow 9:00"));
    const QDateTime local = a->triggerUtc.toLocalTime();
    QCOMPARE(local.time().hour(), 9);
    QCOMPARE(local.time().minute(), 0);
    QCOMPARE(local.date(), QDate::currentDate().addDays(1));

    auto b = AlarmManager::parse(QStringLiteral("today at 23:59 almost"));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("today at 23:59"));
    QCOMPARE(b->label, QStringLiteral("almost"));
    QCOMPARE(b->triggerUtc.toLocalTime().date(), QDate::currentDate());
    QCOMPARE(b->triggerUtc.toLocalTime().time().hour(), 23);
    QCOMPARE(b->triggerUtc.toLocalTime().time().minute(), 59);

    auto c = AlarmManager::parse(QStringLiteral("tomorrow noon"));
    QVERIFY(c.has_value());
    QCOMPARE(c->triggerUtc.toLocalTime().time(), QTime(12, 0));
    QCOMPARE(c->triggerUtc.toLocalTime().date(), QDate::currentDate().addDays(1));
}

void TestParser::relative_weeks_months() {
    auto a = AlarmManager::parse(QStringLiteral("in 2 weeks"));
    QVERIFY(a.has_value());
    const qint64 s = QDateTime::currentDateTimeUtc().secsTo(a->triggerUtc);
    QVERIFY2(s >= 13 * 86400 && s <= 15 * 86400, qPrintable(QString::number(s)));

    auto b = AlarmManager::parse(QStringLiteral("1 week laundry"));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("1 week"));
    QCOMPARE(b->label, QStringLiteral("laundry"));

    auto c = AlarmManager::parse(QStringLiteral("in 1 month"));
    QVERIFY(c.has_value());
    const QDateTime local = c->triggerUtc.toLocalTime();
    const QDateTime now = QDateTime::currentDateTime();
    QCOMPARE(local.date(), now.date().addMonths(1));

    auto d = AlarmManager::parse(QStringLiteral("in 3 years"));
    QVERIFY(d.has_value());
    QCOMPARE(d->triggerUtc.toLocalTime().date(), now.date().addYears(3));

    auto e = AlarmManager::parse(QStringLiteral("1 year warranty"));
    QVERIFY(e.has_value());
    QCOMPARE(e->command, QStringLiteral("1 year"));
    QCOMPARE(e->label, QStringLiteral("warranty"));
}

void TestParser::absolute_date_ampm() {
    auto a = AlarmManager::parse(QStringLiteral("at 2099-10-06 5:00pm"));
    QVERIFY(a.has_value());
    const QDateTime local = a->triggerUtc.toLocalTime();
    QCOMPARE(local.date(), QDate(2099, 10, 6));
    QCOMPARE(local.time().hour(), 17);
    QCOMPARE(local.time().minute(), 0);

    auto b = AlarmManager::parse(QStringLiteral("at 2099-10-06 5pm ship"));
    QVERIFY(b.has_value());
    QCOMPARE(b->label, QStringLiteral("ship"));
    QCOMPARE(b->triggerUtc.toLocalTime().time().hour(), 17);
}

void TestParser::absolute_noon_midnight() {
    auto a = AlarmManager::parse(QStringLiteral("noon"));
    QVERIFY(a.has_value());
    QCOMPARE(a->triggerUtc.toLocalTime().time(), QTime(12, 0));

    auto b = AlarmManager::parse(QStringLiteral("midnight"));
    QVERIFY(b.has_value());
    QCOMPARE(b->triggerUtc.toLocalTime().time(), QTime(0, 0));
}

void TestParser::recurring_monthly() {
    auto a = AlarmManager::parse(QStringLiteral("every month on the 6th at 9:00"));
    QVERIFY(a.has_value());
    QVERIFY(a->recurrence.isRecurring());
    QCOMPARE(a->recurrence.kind, Recurrence::Kind::Monthly);
    QCOMPARE(a->recurrence.dayOfMonth, 6);
    QCOMPARE(a->recurrence.time, QTime(9, 0));
    QVERIFY(a->triggerUtc > QDateTime::currentDateTimeUtc());
    QCOMPARE(a->triggerUtc.toLocalTime().time().hour(), 9);
    QCOMPARE(a->triggerUtc.toLocalTime().date().day(), 6);

    auto b = AlarmManager::parse(QStringLiteral("monthly on 15 at 18:00 rent"));
    QVERIFY(b.has_value());
    QCOMPARE(b->recurrence.dayOfMonth, 15);
    QCOMPARE(b->recurrence.time, QTime(18, 0));
    QCOMPARE(b->label, QStringLiteral("rent"));

    auto c = AlarmManager::parse(QStringLiteral("each month on the 1st 9am"));
    QVERIFY(c.has_value());
    QCOMPARE(c->recurrence.dayOfMonth, 1);
    QCOMPARE(c->recurrence.time, QTime(9, 0));
}

void TestParser::relative_synonyms_combined() {
    auto a = AlarmManager::parse(QStringLiteral("in half an hour"));
    QVERIFY(a.has_value());
    const qint64 s = QDateTime::currentDateTimeUtc().secsTo(a->triggerUtc);
    QVERIFY2(s >= 28 * 60 && s <= 32 * 60, qPrintable(QString::number(s)));

    auto b = AlarmManager::parse(QStringLiteral("in a fortnight"));
    QVERIFY(b.has_value());
    const qint64 sb = QDateTime::currentDateTimeUtc().secsTo(b->triggerUtc);
    QVERIFY2(sb >= 13 * 86400 && sb <= 15 * 86400, qPrintable(QString::number(sb)));

    auto c = AlarmManager::parse(QStringLiteral("in 1 year 2 months"));
    QVERIFY(c.has_value());
    const QDate expect = QDateTime::currentDateTime().date().addYears(1).addMonths(2);
    QCOMPARE(c->triggerUtc.toLocalTime().date(), expect);

    auto d = AlarmManager::parse(QStringLiteral("in 2 weeks 3 days walk"));
    QVERIFY(d.has_value());
    QCOMPARE(d->label, QStringLiteral("walk"));
}

void TestParser::absolute_this_next_week() {
    auto a = AlarmManager::parse(QStringLiteral("this friday 18:00"));
    // May fail if "this friday" already passed this week — still try next monday form
    auto b = AlarmManager::parse(QStringLiteral("next week monday 9:00"));
    QVERIFY(b.has_value());
    QCOMPARE(b->triggerUtc.toLocalTime().date().dayOfWeek(), 1);
    QVERIFY(QDateTime::currentDateTime().daysTo(b->triggerUtc.toLocalTime()) >= 1);

    auto c = AlarmManager::parse(QStringLiteral("next week 10:00"));
    QVERIFY(c.has_value());
    QCOMPARE(c->triggerUtc.toLocalTime().date().dayOfWeek(),
             QDate::currentDate().dayOfWeek());
}

void TestParser::absolute_eod_bare_hour() {
    auto a = AlarmManager::parse(QStringLiteral("tomorrow eod"));
    QVERIFY(a.has_value());
    QCOMPARE(a->triggerUtc.toLocalTime().time(), QTime(23, 59));

    auto b = AlarmManager::parse(QStringLiteral("at 17"));
    QVERIFY(b.has_value());
    QCOMPARE(b->triggerUtc.toLocalTime().time().hour(), 17);
    QCOMPARE(b->triggerUtc.toLocalTime().time().minute(), 0);

    auto c = AlarmManager::parse(QStringLiteral("end of day"));
    QVERIFY(c.has_value());
    QCOMPARE(c->triggerUtc.toLocalTime().time(), QTime(23, 59));
}

void TestParser::recurring_yearly_nth_biweekly() {
    auto a = AlarmManager::parse(QStringLiteral("every year on 10-06 at 9:00"));
    QVERIFY(a.has_value());
    QCOMPARE(a->recurrence.kind, Recurrence::Kind::Yearly);
    QCOMPARE(a->recurrence.month, 10);
    QCOMPARE(a->recurrence.dayOfMonth, 6);
    QCOMPARE(a->recurrence.time, QTime(9, 0));

    auto b = AlarmManager::parse(QStringLiteral("every 2nd tuesday at 18:00"));
    QVERIFY(b.has_value());
    QCOMPARE(b->recurrence.kind, Recurrence::Kind::Monthly);
    QCOMPARE(b->recurrence.weekOrdinal, 2);
    QCOMPARE(b->recurrence.time, QTime(18, 0));

    auto c = AlarmManager::parse(QStringLiteral("every 2 weeks on monday at 9:00"));
    QVERIFY(c.has_value());
    QCOMPARE(c->recurrence.kind, Recurrence::Kind::Weekly);
    QCOMPARE(c->recurrence.weekStride, 2);
    QCOMPARE(c->recurrence.time, QTime(9, 0));
}

void TestParser::absolute_american_ampm() {
    auto a = AlarmManager::parse(QStringLiteral("6:00am"));
    QVERIFY(a.has_value());
    QCOMPARE(a->triggerUtc.toLocalTime().time().hour(), 6);
    QCOMPARE(a->triggerUtc.toLocalTime().time().minute(), 0);

    auto b = AlarmManager::parse(QStringLiteral("6pm"));
    QVERIFY(b.has_value());
    QCOMPARE(b->triggerUtc.toLocalTime().time().hour(), 18);

    auto c = AlarmManager::parse(QStringLiteral("12:00am"));
    QVERIFY(c.has_value());
    QCOMPARE(c->triggerUtc.toLocalTime().time().hour(), 0);

    auto d = AlarmManager::parse(QStringLiteral("12:00pm"));
    QVERIFY(d.has_value());
    QCOMPARE(d->triggerUtc.toLocalTime().time().hour(), 12);

    auto e = AlarmManager::parse(QStringLiteral("6:30 PM"));
    QVERIFY(e.has_value());
    QCOMPARE(e->triggerUtc.toLocalTime().time().hour(), 18);
    QCOMPARE(e->triggerUtc.toLocalTime().time().minute(), 30);
}


void TestParser::absolute_glued_timezone() {
    // Spaced word is a label, not a zone
    auto labeled = AlarmManager::parse(QStringLiteral("at 15:10 CEST"));
    QVERIFY(labeled.has_value());
    QCOMPARE(labeled->label, QStringLiteral("CEST"));
    QCOMPARE(labeled->command, QStringLiteral("at 15:10"));

    auto cest = AlarmManager::parse(QStringLiteral("at 15:10CEST"));
    QVERIFY(cest.has_value());
    QVERIFY(cest->label.isEmpty());
    QCOMPARE(cest->command, QStringLiteral("at 15:10CEST"));
    // 15:10 CEST = 13:10 UTC
    QCOMPARE(cest->triggerUtc.time().hour(), 13);
    QCOMPARE(cest->triggerUtc.time().minute(), 10);

    auto offset = AlarmManager::parse(QStringLiteral("at 15:10+02:00 ship"));
    QVERIFY(offset.has_value());
    QCOMPARE(offset->label, QStringLiteral("ship"));
    QCOMPARE(offset->command, QStringLiteral("at 15:10+02:00"));
    QCOMPARE(offset->triggerUtc.time().hour(), 13);

    auto zulu = AlarmManager::parse(QStringLiteral("12:00Z"));
    QVERIFY(zulu.has_value());
    QCOMPARE(zulu->triggerUtc.time().hour(), 12);
    QCOMPARE(zulu->triggerUtc.time().minute(), 0);

    // am/pm still works (pm is not a zone token)
    auto amp = AlarmManager::parse(QStringLiteral("6:00pm"));
    QVERIFY(amp.has_value());
}

void TestParser::notes_trailing_words() {
    auto a = AlarmManager::parse(QStringLiteral("in 10m stretch"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 10m"));
    QCOMPARE(a->label, QStringLiteral("stretch"));

    auto b = AlarmManager::parse(QStringLiteral("at 15:10 team call"));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("at 15:10"));
    QCOMPARE(b->label, QStringLiteral("team call"));
}

void TestParser::notes_comma() {
    auto a = AlarmManager::parse(QStringLiteral("in 5s, water plants"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5s"));
    QCOMPARE(a->label, QStringLiteral("water plants"));
}

void TestParser::notes_parens_and_quotes() {
    auto a = AlarmManager::parse(QStringLiteral("in 5m (laundry)"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5m"));
    QCOMPARE(a->label, QStringLiteral("laundry"));

    auto b = AlarmManager::parse(QStringLiteral("in 10m \"pick up kids\""));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("in 10m"));
    QCOMPARE(b->label, QStringLiteral("pick up kids"));

    auto c = AlarmManager::parse(QStringLiteral("in 1m 'oven'"));
    QVERIFY(c.has_value());
    QCOMPARE(c->label, QStringLiteral("oven"));
}

void TestParser::explicit_label_overrides_note() {
    auto a = AlarmManager::parse(QStringLiteral("in 10m stretch"), QStringLiteral("forced"));
    QVERIFY(a.has_value());
    QCOMPARE(a->label, QStringLiteral("forced"));
    QCOMPARE(a->command, QStringLiteral("in 10m"));
}

void TestParser::invalid_inputs() {
    QVERIFY(!AlarmManager::parse(QString()).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("   ")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("hello")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at")).has_value());
    // Unit letters must not be the start of an unrelated word
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 5 hamburgers")).has_value());
    // Unsupported calendar units
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 3 decades")).has_value());
    // Absurd durations are rejected instead of overflowing
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 99999999999999999999d")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 999999999999d")).has_value());
}

void TestParser::command_is_time_part_only() {
    auto a = AlarmManager::parse(QStringLiteral("in 2h30m, long walk outside"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 2h30m"));
    QCOMPARE(a->label, QStringLiteral("long walk outside"));
    QVERIFY(a->displayName() == a->label);
}

void TestParser::leading_modifiers_style() {
    auto a = AlarmManager::parse(QStringLiteral("max in 5s"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5s"));
    QVERIFY(a->notificationStyle.has_value());
    QCOMPARE(*a->notificationStyle, NotificationStyle::Fullscreen);
    QVERIFY(!a->disabled);

    auto b = AlarmManager::parse(QStringLiteral("simple in 10m stretch"));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("in 10m"));
    QCOMPARE(b->label, QStringLiteral("stretch"));
    QCOMPARE(*b->notificationStyle, NotificationStyle::Simple);

    auto c = AlarmManager::parse(QStringLiteral("normal at 15:10"));
    QVERIFY(c.has_value());
    QCOMPARE(*c->notificationStyle, NotificationStyle::Standard);

    auto d = AlarmManager::parse(QStringLiteral("silent in 1m"));
    QVERIFY(d.has_value());
    QCOMPARE(*d->notificationStyle, NotificationStyle::Silent);

    auto e = AlarmManager::parse(QStringLiteral("notify in 30s"));
    QVERIFY(e.has_value());
    QCOMPARE(*e->notificationStyle, NotificationStyle::NotifyOnly);

    auto f = AlarmManager::parse(QStringLiteral("fullscreen every 5m"));
    QVERIFY(f.has_value());
    QCOMPARE(*f->notificationStyle, NotificationStyle::Fullscreen);
    QVERIFY(f->recurrence.isRecurring());

    // No modifier → nullopt (app default)
    auto g = AlarmManager::parse(QStringLiteral("in 5m"));
    QVERIFY(g.has_value());
    QVERIFY(!g->notificationStyle.has_value());
}

void TestParser::leading_modifiers_disabled() {
    auto a = AlarmManager::parse(QStringLiteral("disabled in 5m"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5m"));
    QVERIFY(a->disabled);
    QVERIFY(!a->notificationStyle.has_value());

    auto b = AlarmManager::parse(QStringLiteral("paused every monday at 9:00"));
    QVERIFY(b.has_value());
    QVERIFY(b->disabled);
    QVERIFY(b->recurrence.isRecurring());
}

void TestParser::leading_modifiers_combined() {
    auto a = AlarmManager::parse(QStringLiteral("disabled silent in 5m note"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5m"));
    QCOMPARE(a->label, QStringLiteral("note"));
    QVERIFY(a->disabled);
    QCOMPARE(*a->notificationStyle, NotificationStyle::Silent);

    // Last style wins
    auto b = AlarmManager::parse(QStringLiteral("simple max in 10s"));
    QVERIFY(b.has_value());
    QCOMPARE(*b->notificationStyle, NotificationStyle::Fullscreen);

    // Trailing "simple" is a label, not a modifier
    auto c = AlarmManager::parse(QStringLiteral("in 5m simple"));
    QVERIFY(c.has_value());
    QCOMPARE(c->command, QStringLiteral("in 5m"));
    QCOMPARE(c->label, QStringLiteral("simple"));
    QVERIFY(!c->notificationStyle.has_value());
}

struct RelCase {
    const char* input;
    qint64 secs;
    const char* command;
    const char* label;
};

/** Parse each case; check offset (±2 s), command and label. */
static void checkRelative(const RelCase* begin, const RelCase* end) {
    for (const RelCase* c = begin; c != end; ++c) {
        auto a = AlarmManager::parse(QString::fromUtf8(c->input));
        QVERIFY2(a.has_value(), c->input);
        const qint64 s = secsUntil(*a);
        QVERIFY2(s >= c->secs - 2 && s <= c->secs,
                 qPrintable(QStringLiteral("%1: %2 s, expected %3")
                                .arg(QString::fromUtf8(c->input)).arg(s).arg(c->secs)));
        QCOMPARE(a->command, QString::fromUtf8(c->command));
        QCOMPARE(a->label, QString::fromUtf8(c->label));
        QVERIFY(a->recurrence.kind == Recurrence::Kind::None);
    }
}

void TestParser::relative_clock_style() {
    const RelCase cases[] = {
        {"in 5:10:00", 5 * 3600 + 10 * 60, "in 5:10:00", ""},
        {"in 5:10", 5 * 3600 + 10 * 60, "in 5:10", ""},
        {"in 0:45", 45 * 60, "in 0:45", ""},
        {"in 0:00:30", 30, "in 0:00:30", ""},
        {"in 1:30:15", 3600 + 30 * 60 + 15, "in 1:30:15", ""},
        {"in 100:00", 100 * 3600, "in 100:00", ""},
        {"in 5:10 tea", 5 * 3600 + 10 * 60, "in 5:10", "tea"},
        {"in 0:20, pasta", 20 * 60, "in 0:20", "pasta"},
        {"IN 0:05", 5 * 60, "IN 0:05", ""},
    };
    checkRelative(std::begin(cases), std::end(cases));

    // Out-of-range fields and zero are rejected
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 5:60")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 1:00:60")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 0:00")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 5:1")).has_value());
}

void TestParser::relative_decimal_amounts() {
    const RelCase cases[] = {
        {"in 1.5h", 90 * 60, "in 1.5h", ""},
        {"in 2.5 hours", 150 * 60, "in 2.5 hours", ""},
        {"in 0.5d", 12 * 3600, "in 0.5d", ""},
        {"in 1.5 days", 36 * 3600, "in 1.5 days", ""},
        {"in 2.5m", 150, "in 2.5m", ""},
        {"1.25h", 75 * 60, "1.25h", ""},
        {"in 1.5h laundry", 90 * 60, "in 1.5h", "laundry"},
    };
    checkRelative(std::begin(cases), std::end(cases));

    auto e = AlarmManager::parse(QStringLiteral("every 1.5h"));
    QVERIFY(e.has_value());
    QCOMPARE(e->recurrence.kind, Recurrence::Kind::Interval);
    QCOMPARE(e->recurrence.intervalSecs, 90 * 60);

    QVERIFY(!AlarmManager::parse(QStringLiteral("in 1.5 hamburgers")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 1.h")).has_value());
}

void TestParser::relative_implicit_trailing_unit() {
    const RelCase cases[] = {
        {"in 1h30", 90 * 60, "in 1h30", ""},
        {"in 2m30", 150, "in 2m30", ""},
        {"in 1d2h30", 86400 + 2 * 3600 + 30 * 60, "in 1d2h30", ""},
        {"1h05", 65 * 60, "1h05", ""},
        {"in 1h30 tea", 90 * 60, "in 1h30", "tea"},
        // Explicit unit still wins over the implicit one
        {"in 1h30m", 90 * 60, "in 1h30m", ""},
        // Not glued: the number is a note, not minutes
        {"in 5m 2 eggs", 5 * 60, "in 5m", "2 eggs"},
    };
    checkRelative(std::begin(cases), std::end(cases));

    auto e = AlarmManager::parse(QStringLiteral("every 1h30 stretch"));
    QVERIFY(e.has_value());
    QCOMPARE(e->recurrence.intervalSecs, 90 * 60);
    QCOMPARE(e->label, QStringLiteral("stretch"));

    // Implicit part must be < 60, not decimal-based, not combined with explicit unit
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 1h60")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 1.5h30")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 1h30 5m")).has_value());
}

void TestParser::relative_and_connector() {
    const RelCase cases[] = {
        {"in 1 hour and 30 minutes", 90 * 60, "in 1 hour and 30 minutes", ""},
        {"in 5m and 30s", 5 * 60 + 30, "in 5m and 30s", ""},
        {"in 1 day and 2 hours", 26 * 3600, "in 1 day and 2 hours", ""},
        {"in 1 hour and 30 minutes stretch", 90 * 60, "in 1 hour and 30 minutes", "stretch"},
        // "and" not followed by a unit stays in the note
        {"in 5m and then stretch", 5 * 60, "in 5m", "and then stretch"},
    };
    checkRelative(std::begin(cases), std::end(cases));

    auto w = AlarmManager::parse(QStringLiteral("in 2 weeks and 3 days walk"));
    QVERIFY(w.has_value());
    QCOMPARE(w->command, QStringLiteral("in 2 weeks and 3 days"));
    QCOMPARE(w->label, QStringLiteral("walk"));
    QCOMPARE(w->triggerUtc.toLocalTime().date(), QDate::currentDate().addDays(17));

    auto y = AlarmManager::parse(QStringLiteral("in 1 year and 2 months"));
    QVERIFY(y.has_value());
    QCOMPARE(y->triggerUtc.toLocalTime().date(),
             QDateTime::currentDateTime().date().addYears(1).addMonths(2));

    // Calendar units followed by clock units now split correctly
    auto c = AlarmManager::parse(QStringLiteral("in 1 week 2h review"));
    QVERIFY(c.has_value());
    QCOMPARE(c->command, QStringLiteral("in 1 week 2h"));
    QCOMPARE(c->label, QStringLiteral("review"));
}

void TestParser::relative_word_units() {
    const RelCase cases[] = {
        {"in a minute", 60, "in a minute", ""},
        {"in a second", 1, "in a second", ""},
        {"in an hour", 3600, "in an hour", ""},
        {"in a min", 60, "in a min", ""},
        {"a minute", 60, "a minute", ""},
        {"in an hour and a half", 90 * 60, "in an hour and a half", ""},
        {"in one hour and a half", 90 * 60, "in one hour and a half", ""},
        {"in a minute check oven", 60, "in a minute", "check oven"},
        {"in an hour and a half call mom", 90 * 60, "in an hour and a half", "call mom"},
        {"in half an hour tea", 30 * 60, "in half an hour", "tea"},
        {"in a quarter hour", 15 * 60, "in a quarter hour", ""},
    };
    checkRelative(std::begin(cases), std::end(cases));

    auto d = AlarmManager::parse(QStringLiteral("in a day"));
    QVERIFY(d.has_value());
    QCOMPARE(d->triggerUtc.toLocalTime().date(), QDate::currentDate().addDays(1));
    auto w = AlarmManager::parse(QStringLiteral("in a week"));
    QVERIFY(w.has_value());
    QCOMPARE(w->triggerUtc.toLocalTime().date(), QDate::currentDate().addDays(7));
    auto m = AlarmManager::parse(QStringLiteral("in a month rent"));
    QVERIFY(m.has_value());
    QCOMPARE(m->triggerUtc.toLocalTime().date(),
             QDateTime::currentDateTime().date().addMonths(1));
    QCOMPARE(m->label, QStringLiteral("rent"));
    auto y = AlarmManager::parse(QStringLiteral("in a year"));
    QVERIFY(y.has_value());
    QCOMPARE(y->triggerUtc.toLocalTime().date(),
             QDateTime::currentDateTime().date().addYears(1));

    QVERIFY(!AlarmManager::parse(QStringLiteral("in a while")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in a minutely")).has_value());
}

void TestParser::relative_from_now() {
    const RelCase cases[] = {
        {"in 5 minutes from now", 5 * 60, "in 5 minutes from now", ""},
        {"5m from now", 5 * 60, "5m from now", ""},
        {"in 1:30 from now", 90 * 60, "in 1:30 from now", ""},
        {"an hour from now", 3600, "an hour from now", ""},
        {"in 10m from now stretch", 10 * 60, "in 10m from now", "stretch"},
        // "from" not followed by "now" is a note
        {"in 10m from mom", 10 * 60, "in 10m", "from mom"},
    };
    checkRelative(std::begin(cases), std::end(cases));
}

void TestParser::absolute_dot_and_four_digit() {
    const struct {
        const char* input;
        QTime time;
        const char* command;
        const char* label;
    } cases[] = {
        {"at 15.10", QTime(15, 10), "at 15.10", ""},
        {"15.10", QTime(15, 10), "15.10", ""},
        {"at 3.30pm", QTime(15, 30), "at 3.30pm", ""},
        {"at 3.30 p.m.", QTime(15, 30), "at 3.30 p.m.", ""},
        {"at 6:30 p.m.", QTime(18, 30), "at 6:30 p.m.", ""},
        {"at 0730", QTime(7, 30), "at 0730", ""},
        {"at 1510", QTime(15, 10), "at 1510", ""},
        {"at 2359", QTime(23, 59), "at 2359", ""},
        {"at 15.10 team call", QTime(15, 10), "at 15.10", "team call"},
        {"at 0730 gym", QTime(7, 30), "at 0730", "gym"},
        {"tomorrow 15.10", QTime(15, 10), "tomorrow 15.10", ""},
        {"tomorrow at 0730 dentist", QTime(7, 30), "tomorrow at 0730", "dentist"},
        {"next monday 18.00", QTime(18, 0), "next monday 18.00", ""},
    };
    for (const auto& c : cases) {
        auto a = AlarmManager::parse(QString::fromUtf8(c.input));
        QVERIFY2(a.has_value(), c.input);
        QCOMPARE(a->triggerUtc.toLocalTime().time(), c.time);
        QCOMPARE(a->command, QString::fromUtf8(c.command));
        QCOMPARE(a->label, QString::fromUtf8(c.label));
    }

    auto d = AlarmManager::parse(QStringLiteral("at 2099-06-15 17.30"));
    QVERIFY(d.has_value());
    QCOMPARE(d->triggerUtc.toLocalTime(), QDateTime(QDate(2099, 6, 15), QTime(17, 30)));
    auto dn = AlarmManager::parse(QStringLiteral("at 2099-06-15 0930 checkup"));
    QVERIFY(dn.has_value());
    QCOMPARE(dn->triggerUtc.toLocalTime(), QDateTime(QDate(2099, 6, 15), QTime(9, 30)));
    QCOMPARE(dn->label, QStringLiteral("checkup"));

    auto z = AlarmManager::parse(QStringLiteral("at 1200Z"));
    QVERIFY(z.has_value());
    QCOMPARE(z->triggerUtc.time(), QTime(12, 0));

    auto w = AlarmManager::parse(QStringLiteral("every monday at 18.00 laundry"));
    QVERIFY(w.has_value());
    QCOMPARE(w->recurrence.kind, Recurrence::Kind::Weekly);
    QCOMPARE(w->recurrence.time, QTime(18, 0));
    QCOMPARE(w->label, QStringLiteral("laundry"));
    auto dl = AlarmManager::parse(QStringLiteral("daily at 0730"));
    QVERIFY(dl.has_value());
    QCOMPARE(dl->recurrence.time, QTime(7, 30));

    QVERIFY(!AlarmManager::parse(QStringLiteral("at 15.60")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at 2460")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at 15.1")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at 13.30pm")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at 12345")).has_value());
    // An invalid date must not degrade into "20:99"-style time + note
    QVERIFY(!AlarmManager::parse(QStringLiteral("at 2099-13-45 9:00")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at 2099-06-15 garbage")).has_value());
}

void TestParser::tolerant_forms_no_conflicts() {
    // "H:MM" without "in" is a time of day, with "in" a duration.
    auto abs = AlarmManager::parse(QStringLiteral("5:10"));
    QVERIFY(abs.has_value());
    QCOMPARE(abs->triggerUtc.toLocalTime().time(), QTime(5, 10));
    auto at = AlarmManager::parse(QStringLiteral("at 5:10"));
    QVERIFY(at.has_value());
    QCOMPARE(at->triggerUtc.toLocalTime().time(), QTime(5, 10));
    auto rel = AlarmManager::parse(QStringLiteral("in 5:10"));
    QVERIFY(rel.has_value());
    const qint64 s = secsUntil(*rel);
    QVERIFY(s >= 5 * 3600 + 10 * 60 - 2 && s <= 5 * 3600 + 10 * 60);

    // "every H:MM" is ambiguous (interval vs. daily time) and stays rejected.
    QVERIFY(!AlarmManager::parse(QStringLiteral("every 1:30")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at in 5:10")).has_value());

    // Leading modifiers combine with the new forms.
    auto mod = AlarmManager::parse(QStringLiteral("max in 1:30 tea"));
    QVERIFY(mod.has_value());
    QCOMPARE(mod->notificationStyle, std::optional(NotificationStyle::Fullscreen));
    QCOMPARE(mod->command, QStringLiteral("in 1:30"));
    QCOMPARE(mod->label, QStringLiteral("tea"));
    auto mod2 = AlarmManager::parse(QStringLiteral("silent in a minute"));
    QVERIFY(mod2.has_value());
    QCOMPARE(mod2->notificationStyle, std::optional(NotificationStyle::Silent));

    // Existing forms keep their meaning.
    auto h = AlarmManager::parse(QStringLiteral("in 5 hamburgers"));
    QVERIFY(!h.has_value());
    auto st = AlarmManager::parse(QStringLiteral("in 10m stretch"));
    QVERIFY(st.has_value());
    QCOMPARE(st->label, QStringLiteral("stretch"));
    auto bare = AlarmManager::parse(QStringLiteral("at 17"));
    QVERIFY(bare.has_value());
    QCOMPARE(bare->triggerUtc.toLocalTime().time(), QTime(17, 0));
    auto ampm = AlarmManager::parse(QStringLiteral("6 p.m."));
    QVERIFY(ampm.has_value());
    QCOMPARE(ampm->triggerUtc.toLocalTime().time(), QTime(18, 0));
    auto zone = AlarmManager::parse(QStringLiteral("at 15:10 CEST"));
    QVERIFY(zone.has_value());
    QCOMPARE(zone->label, QStringLiteral("CEST"));
    auto iv = AlarmManager::parse(QStringLiteral("every 5m and stretch"));
    QVERIFY(iv.has_value());
    QCOMPARE(iv->recurrence.intervalSecs, 5 * 60);
    QCOMPARE(iv->label, QStringLiteral("and stretch"));
    auto hr = AlarmManager::parse(QStringLiteral("every hour"));
    QVERIFY(hr.has_value());
    QCOMPARE(hr->recurrence.intervalSecs, 3600);
}

QTEST_MAIN(TestParser)
#include "test_parser.moc"
