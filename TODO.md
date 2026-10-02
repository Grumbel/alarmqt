# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-007.1-notify-label-sound-071e920.bundle`
- **Next bundle NNN:** 008

## Done in this line

- Core app, desktop integration, short AppStream id, CLI IPC fix, table UI
- Notification dialog: readable text, side blink bars only, alarm sound (WAV + beep fallback)
- Optional alarm notes: `in 5m (kitchen)`, `in 5m "tea"`

## Feature brainstorm (not implemented)

### High value
- [ ] **Mute / volume** for notification sound; “silent alarm” toggle
- [ ] **Custom snooze** input (type minutes) + remember last snooze
- [ ] **Recurring alarms** daily/weekly (flag already on `Alarm`)
- [ ] **Pause all** / “do not disturb” until time
- [ ] **Export/import** alarms JSON; optional CLI `--export`
- [ ] **History** of acknowledged alarms (instead of pruning immediately)

### UX
- [ ] Double-click row to edit note or reschedule
- [ ] Drag-reorder or pin important alarms
- [ ] Progress bar / circular countdown for next alarm in tray menu
- [ ] Global hotkey to add alarm (e.g. Super+A) via optional daemon flag
- [ ] Dark/light follow system theme (currently fixed dark notification)

### Integration
- [ ] Freedesktop Notifications portal as alternative to custom dialog
- [ ] PipeWire/Pulse status (don’t fire loud during meeting — hard)
- [ ] `systemd --user` timer generation for offline reliability (optional)

### Reliability
- [ ] Unit tests for parser (relative, absolute, notes)
- [ ] `--list` reply channel from primary
- [ ] Detect clock jumps (suspend/resume) and re-evaluate due alarms immediately

## Handoff

```bash
git pull /path/to/alarmqt-007.1-notify-label-sound-071e920.bundle HEAD
```
