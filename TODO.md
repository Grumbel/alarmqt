# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-012.1-clear-done-docs-071e920.bundle`
- **Next bundle NNN:** 013

## Status

**v0.1 feature set is complete** for the original brief (tray alarms, CLI, ack until done,
notes, edit/restart, desktop integration, Nix flake).

## Done (this line)

See `git log`. Highlights: single-instance CLI, table UI, command/label split, restart,
DONE retention, current clock, notification sound + side blinkers, PipeWire wrap, Clear DONE.

## Optional later (not blocking)

- [ ] Mute / volume for alarm sound
- [ ] Daily/weekly recurrence
- [ ] Parser unit tests
- [ ] Freedesktop notification portal alternative
- [ ] `--list` machine-readable format

## Handoff

```bash
git pull /path/to/alarmqt-012.1-clear-done-docs-071e920.bundle HEAD
```
