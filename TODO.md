# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** Leading modifiers + Silent / NotifyOnly styles
- **Next bundle NNN:** 049

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] Recurring alarms: `until` / count limits
- [ ] Shell completions (bash/zsh/fish)

## Done recently

- Leading modifiers on add/parse: `simple` / `normal`|`standard` / `max`|`fullscreen` /
  `silent` / `notify`|`tray` / `disabled`|`paused` at the start of the expression;
  stored as per-alarm `notificationStyle` or `disabled` (command keeps time-only)
- New styles: **Silent** (Standard blinkers, no sound), **NotifyOnly** (tray message
  only); Edit dialog + Settings menu + JSON + syntax help + man page
- Per-alarm notification style: Edit alarm dropdown (Default / Standard /
  Simple / Fullscreen / Silent / Notify only); JSON when set; nullopt → Settings
- Focus alarm input: Ctrl+N and Ctrl+L (Add alarm…)
- Delete / Clear DONE confirm uses QDialogButtonBox (Cancel + Delete
  roles) for the same platform button order as other dialogs
- Alarm notification buttons use QDialogButtonBox roles (Snooze as
  Action, Acknowledge as Accept) for platform order
- Fullscreen notification keeps the ringing-clock SVG aspect ratio (no
  stretch to square)
- Dialog buttons use Qt standard roles so Cancel/Ok (and Cancel/Delete)
  follow the platform order (GNOME Cancel then action; Windows may reverse)
- Interval auto-roll: unanswered `every Nm` notification closes when the
  next slot is due and a fresh one opens (latest due slot)
- Mute sound (Ctrl+M) and Volume menu (100/75/50/25/10%) in Settings;
  applied to notification playback
- Notification styles (Settings): Standard, Simple, Fullscreen, Silent,
  Notification only — persisted in QSettings
- Delete confirmation uses Delete/Cancel (not Yes/No)
- Edit menu with Undo/Redo (Ctrl+Z / Ctrl+Shift+Z) for list mutations;
  Edit alarm stays under Edit
- Classic colorful SVG icons on main menu, context menu, and tray menu
- Main menu bar (File / Alarm / Help) with standard actions and About
- Disable / Enable alarms (right-click, Alarm menu, Ctrl+P): paused state
  never fires; status DISABLED; gray row; persists in JSON; restart clears it
- Relative / absolute / recurring parser expansions (years, this/next week,
  eod, Nth weekday, biweekly, synonyms, …)

## Handoff

Apply the latest `alarmqt-048.*.bundle` from artifacts (cumulative from `071e920`).
