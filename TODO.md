# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** see `git log -1 --oneline` after applying latest bundle
- **Next bundle NNN:** 002 (001 was the initial checkin on GitHub)

## Done in this line

- Initial QWidget app: tray, single-instance, relative/absolute parse, JSON persistence, ack/snooze dialog
- Continuity docs (`AGENTS.md`, `TODO.md`)
- REUSE / SPDX / LICENSE
- Re-notify loop while alarm is triggered and unacknowledged
- Prune acknowledged alarms on save
- flake homepage + minor polish

## Open / nice-to-have

- [ ] Sound on trigger (optional, e.g. `QSoundEffect` or `paplay`)
- [ ] Daily / weekly repeat (data model already has `repeating` flag)
- [ ] Snooze presets configurable
- [ ] Tray icon tint when alarm is due soon
- [ ] `--list` should print from primary via reply protocol (currently secondary only hints)
- [ ] Unit tests for the time parser
- [ ] Install icon/desktop file paths verified under Nix
- [ ] Confirm build on NixOS (sandbox had no `nix` / Qt)

## Known limitations

- Parser is regex-based, not full natural language (“next Tuesday”).
- No network time / NTP awareness beyond OS clock.
- Acknowledged alarms are dropped on next save (no history UI).

## Handoff

Anyone continuing: read `AGENTS.md`, this file, then `git log`. Apply the latest `alarmqt-NNN.*.bundle` from artifacts if not already at tip.
