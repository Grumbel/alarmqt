# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-008.1-clock-edit-done-071e920.bundle`
- **Next bundle NNN:** 009

## Done recently

- Prominent current time display
- Alarms stay after ack as **DONE** (persisted; no auto-prune)
- Edit alarm (label, when, done flag) — button / Ctrl+E / double-click
- Remove confirms; table Status column ACTIVE / DUE / DONE

## Open / nice-to-have

- [ ] Mute / volume for notification sound
- [ ] Recurring daily/weekly
- [ ] Reactivate DONE alarm with one click
- [ ] Parser unit tests
- [ ] `--list` includes DONE with flag

## Handoff

```bash
git pull /path/to/alarmqt-008.1-clock-edit-done-071e920.bundle HEAD
```
