# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-009.1-command-label-restart-071e920.bundle`
- **Next bundle NNN:** 010

## Done recently

- Split **command** (time expression) and **label** (optional note)
- Table columns: Status | Remaining | When | Command | Label
- **Restart** re-parses command from now (button, Ctrl+R, context menu)
- Edit dialog edits command + label + when + done

## Open

- [ ] Mute sound
- [ ] Recurring alarms
- [ ] One-click reactivate without re-parsing (keep same absolute time if future)

## Handoff

```bash
git pull /path/to/alarmqt-009.1-command-label-restart-071e920.bundle HEAD
```
