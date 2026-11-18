# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** Edit alarm dialog overhaul (single time expression + live preview)
- **Next bundle NNN:** 050

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] Recurring alarms: `until` / count limits
- [ ] Shell completions (bash/zsh/fish)

## Done recently

- Edit alarm dialog overhaul: the Command / When / "Apply command → When"
  trio is replaced by one **Time** expression (main-input syntax) with a live
  preview (local time, countdown, repeat rule, overdue warning; OK disabled on
  parse errors) and a calendar picker that writes `yyyy-MM-dd HH:mm` into the
  expression. Trailing note / leading modifiers move into Label / Notification /
  State. **State** combo (Active / Disabled / Done) replaces the Done checkbox.
  Untouched expression keeps the schedule (incl. snooze, ringing dialog);
  reopening a DONE alarm re-evaluates like Restart; explicit past dates are no
  longer silently rolled forward by days. Logic in `AlarmEdit`, tests in
  `tests/test_edit.cpp`
- Desktop notifications go straight to `org.freedesktop.Notifications`
  (`app_icon=alarmqt`, `image-data` fallback when the icon is not installed,
  `desktop-entry` hint) so the AlarmQt logo shows instead of the warning symbol;
  Qt's tray balloon only as fallback (Qt's D-Bus tray drops custom icons).
  Notify-only alarms no longer post the notification twice
- Tolerant time parsing: `in 5:10` / `in 5:10:00` (h:mm[:ss], only after `in`),
  decimals (`in 1.5h`), glued implicit unit (`in 1h30`, `in 2m30`), `and`
  between units, `in a minute` / `a day` / `a week` / `an hour and a half`,
  trailing `from now`; absolute `at 15.10`, `at 3.30pm`, `at 0730` (also in
  today/weekday/recurring forms); `every 1:30` stays rejected (ambiguous);
  out-of-range calendar amounts (`in 200 years`) now fail instead of being ignored

- Notification dialog shows live local **Now** time and **Overdue by** duration
  (seconds/minutes/hours past scheduled time), updated every second; Standard,
  Simple, and Fullscreen layouts
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

Apply the latest `alarmqt-049.*.bundle` from artifacts (cumulative from `071e920`).
