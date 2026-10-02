# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD after missed/snooze/notification work (see latest bundle)
- **Next bundle NNN:** 022

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

Recent: missed-alarm indication, SNOOZED status with separate scheduled vs snooze time, faster alternating black/red notification squares, table row blink until ack.

## Open

- [ ] **Recurring alarms** — daily / weekly (and maybe custom interval). Data model already has a `repeating` flag; needs UX (edit dialog + list status), schedule logic after ack/fire, and persistence of recurrence rule.
- [ ] Mute / volume for notification sound
- [ ] Shell completions (bash/zsh/fish)
- [ ] `--list` reply protocol when talking to a running primary

## Done recently

- Missed alarms (app not running), SNOOZED status, notification blink redesign, row blink
- Parser unit tests (Qt Test) + American 12-hour coverage
- Man page, friendly notes, verification fixes, VERSION scheme
- Resizable/reorderable table columns

## Handoff

Apply the latest `alarmqt-021.*.bundle` from artifacts (cumulative from `071e920`).
