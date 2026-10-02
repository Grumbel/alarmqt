# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD after full-height notify strips (see latest bundle)
- **Next bundle NNN:** 027

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] **Recurring alarms** — daily / weekly (and maybe custom interval). Data model already has a `repeating` flag; needs UX (edit dialog + list status), schedule logic after ack/fire, and persistence of recurrence rule.
- [ ] Mute / volume for notification sound
- [ ] Shell completions (bash/zsh/fish)

## Done recently

- Session D-Bus API (`alarmqt.app` / `/alarmqt` / `alarmqt.App`)
- `--list` reply protocol (secondary prints primary's alarm list)
- Missed alarms, SNOOZED status, notification blink redesign, row blink
- Parser unit tests (Qt Test) + American 12-hour coverage
- Man page, friendly notes, VERSION scheme
- Resizable/reorderable table columns

## Handoff

Apply the latest `alarmqt-022.*.bundle` from artifacts (cumulative from `071e920`).
