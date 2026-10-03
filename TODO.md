# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** Help button / F1 syntax dialog (this session)
- **Next bundle NNN:** 030

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] Recurring alarms, follow-ups: auto-close an unanswered interval
      notification when the next one is due; pause/resume; `until` / count
      limits; monthly rules
- [ ] Mute / volume for notification sound
- [ ] Shell completions (bash/zsh/fish)

## Done recently

- Help button + F1: full alarm time syntax dialog (relative, absolute,
  glued zones, repeating, notes) with examples; parse-error hint points here
- Recurring alarms: `every 5m`, `every monday at 18:00`, `daily at 7:30`,
  weekdays/weekends; re-arm on ack, Skip next (Ctrl+K), DST-safe
- Fix glued timezone parsing (`at 15:10CEST`)
- Man page / README: D-Bus + busctl examples
- Session D-Bus API (`org.alarmqt.AlarmQt` / `/org/alarmqt/AlarmQt`)
- `--list` reply protocol (secondary prints primary's alarm list)
- Missed alarms, SNOOZED status, notification blink redesign, row blink
- Parser unit tests (Qt Test) + American 12-hour coverage
- Man page, friendly notes, VERSION scheme
- Resizable/reorderable table columns

## Handoff

Apply the latest `alarmqt-029.*.bundle` from artifacts (cumulative from `071e920`).
