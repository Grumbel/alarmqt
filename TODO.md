# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-004.1-desktop-integration-071e920.bundle`
- **Next bundle NNN:** 005

## Done in this line

- Initial QWidget app: tray, single-instance, relative/absolute parse, JSON persistence, ack/snooze dialog (`071e920`)
- Continuity docs (`AGENTS.md`, `TODO.md`)
- REUSE / SPDX / LICENSE (GPL-3.0-or-later)
- Re-notify loop while alarm is triggered and unacknowledged
- Prune acknowledged alarms on load/save
- Fix QShortcut compile error on Qt 6.11 (member ptr → lambda)
- Cute anime SVG icon; full Linux desktop integration (.desktop, AppStream, hicolor icon install, setDesktopFileName, --raise)

## Open / nice-to-have

- [ ] Sound on trigger (optional, e.g. `QSoundEffect` or `paplay`)
- [ ] Daily / weekly repeat (data model already has `repeating` flag)
- [ ] Snooze presets configurable
- [ ] Tray icon tint when alarm is due soon
- [ ] `--list` should print from primary via reply protocol (currently secondary only hints)
- [ ] Unit tests for the time parser
- [ ] PNG fallbacks for icon themes that ignore SVG (optional)
- [ ] `update-desktop-database` / icon cache note for non-Nix packaging

## Known limitations

- Parser is regex-based, not full natural language (“next Tuesday”).
- No network time / NTP awareness beyond OS clock.
- Acknowledged alarms are dropped on next save (no history UI).

## Handoff

```bash
git pull /path/to/alarmqt-004.1-desktop-integration-071e920.bundle HEAD
```

Requires base `071e920`. Read `AGENTS.md` and this file.
