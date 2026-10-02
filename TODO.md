# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-020.1-parser-tests-071e920.bundle`
- **Next bundle NNN:** 021

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] **Recurring alarms** — daily / weekly (and maybe custom interval). Data model already has a `repeating` flag; needs UX (edit dialog + list status), schedule logic after ack/fire, and persistence of recurrence rule.
- [ ] Mute / volume for notification sound
- [ ] Shell completions (bash/zsh/fish)
- [ ] `--list` reply protocol when talking to a running primary

## Done recently

- Parser unit tests (Qt Test)
- Man page, friendly notes, verification fixes, VERSION scheme

## Handoff

```bash
git pull /path/to/alarmqt-020.1-parser-tests-071e920.bundle HEAD
```
