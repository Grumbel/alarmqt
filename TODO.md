# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** monthly recurrence (`every month on the 6th at 9:00`)
- **Next bundle NNN:** 035

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] Recurring alarms, follow-ups: auto-close an unanswered interval
      notification when the next one is due; pause/resume; `until` / count
      limits
- [ ] Mute / volume for notification sound
- [ ] Shell completions (bash/zsh/fish)

## Done recently

- Monthly recurrence: `every month on the 6th at 9:00`, `monthly on 15 at 18:00`,
  `each month on the 1st 9am` — local wall-clock; months lacking that day are
  skipped (e.g. 31st in February)
- Parser: `today`/`tomorrow` + time; bare weekday (`monday 9:00`, `next` optional);
  `in 2 weeks` / `in 1 month`; full date + am/pm (`at 2026-10-06 5:00pm`);
  `noon` / `midnight` as time tokens
- `next monday 5:50pm` / `next mon at 9:00` — one-shot absolute at the next
  matching weekday (local wall-clock, same DST rules as weekly recurrence);
  optional note; not recurring
- Full-date absolute times accept a single-digit hour (`at 2099-10-06 5:00`,
  `…T5:00:00`) — same as time-only `H:mm`; previously only zero-padded `HH`
- Big clock shrinks to fit instead of cropping on resize; displayed times
  (clock, table, notification, `--list`, CLI add) drop the zone suffix —
  always local
- Notification dialog: animated ringing clock (`icons/alarm-ringing.svg`,
  SMIL, played via QSvgRenderer) — hops side to side, rattles its bells,
  radiates sound waves
- Minimal input row: drop Add/Edit/Restart/Remove toolbar buttons; keep
  compact **?** for syntax help (F1). Actions via Enter, shortcuts, double-click,
  context menu
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

Apply the latest `alarmqt-034.*.bundle` from artifacts (cumulative from `071e920`).
