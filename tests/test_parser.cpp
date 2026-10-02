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
    void absolute_american_ampm();
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
    QCOMPARE(b->label, QStringLiteral("water plants"));
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

void TestParser::notes_trailing_words() {
    auto a = AlarmManager::parse(QStringLiteral("in 10m stretch"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5m"));
    QCOMPARE(a->label, QStringLiteral("stretch"));

    auto b = AlarmManager::parse(QStringLiteral("at 15:10 team call"));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("at 15:10"));
    QCOMPARE(b->label, QStringLiteral("team call"));
}

void TestParser::notes_comma() {
    auto a = AlarmManager::parse(QStringLiteral("in 5s, tea"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5s"));
    QCOMPARE(a->label, QStringLiteral("water plants"));
}

void TestParser::notes_parens_and_quotes() {
    auto a = AlarmManager::parse(QStringLiteral("in 5m (laundry)"));
    QVERIFY(a.has_value());
    QCOMPARE(a->command, QStringLiteral("in 5m"));
    QCOMPARE(a->label, QStringLiteral("stretch"));

    auto b = AlarmManager::parse(QStringLiteral("in 10m \"tea\""));
    QVERIFY(b.has_value());
    QCOMPARE(b->command, QStringLiteral("in 10m"));
    QCOMPARE(b->label, QStringLiteral("water plants"));

    auto c = AlarmManager::parse(QStringLiteral("in 1m 'oven'"));
    QVERIFY(c.has_value());
    QCOMPARE(c->label, QStringLiteral("oven"));
}

void TestParser::explicit_label_overrides_note() {
    auto a = AlarmManager::parse(QStringLiteral("in 10m stretch"), QStringLiteral("forced"));
    QVERIFY(a.has_value());
    QCOMPARE(a->label, QStringLiteral("forced"));
    QCOMPARE(a->command, QStringLiteral("in 5m"));
}

void TestParser::invalid_inputs() {
    QVERIFY(!AlarmManager::parse(QString()).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("   ")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("hello")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("at")).has_value());
    // Unit letters must not be the start of an unrelated word
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 5 hamburgers")).has_value());
    QVERIFY(!AlarmManager::parse(QStringLiteral("in 3 months")).has_value());
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
