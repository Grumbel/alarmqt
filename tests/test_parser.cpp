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
    void absolute_american_ampm();
    void absolute_glued_timezone();
    void notes_trailing_words();
    void notes_comma();
    void notes_parens_and_quotes();
    void explicit_label_overrides_note();
    void invalid_inputs();
    void command_is_time_part_only();
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

QTEST_MAIN(TestParser)
#include "test_parser.moc"
