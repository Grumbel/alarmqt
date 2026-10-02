# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-002.2-continuity-renotify-071e920.bundle`
- **Next bundle NNN:** 003

## Done in this line

- Initial QWidget app: tray, single-instance, relative/absolute parse, JSON persistence, ack/snooze dialog (`071e920`)
- Continuity docs (`AGENTS.md`, `TODO.md`)
- REUSE / SPDX / LICENSE (GPL-3.0-or-later)
- Re-notify loop while alarm is triggered and unacknowledged
- Prune acknowledged alarms on load/save
- flake homepage + README wording

## Open / nice-to-have

- [ ] Sound on trigger (optional, e.g. `QSoundEffect` or `paplay`)
- [ ] Daily / weekly repeat (data model already has `repeating` flag)
- [ ] Snooze presets configurable
- [ ] Tray icon tint when alarm is due soon
- [ ] `--list` should print from primary via reply protocol (currently secondary only hints)
- [ ] Unit tests for the time parser
- [ ] Install icon/desktop file paths verified under Nix
- [ ] Confirm build on NixOS (agent sandbox had no `nix` / Qt)

## Known limitations

- Parser is regex-based, not full natural language (“next Tuesday”).
- No network time / NTP awareness beyond OS clock.
- Acknowledged alarms are dropped on next save (no history UI).

## Handoff

Anyone continuing: read `AGENTS.md`, this file, then `git log`. Apply the latest `alarmqt-NNN.*.bundle` from artifacts if not already at tip:

```bash
git pull /path/to/alarmqt-002.2-continuity-renotify-071e920.bundle HEAD
```
