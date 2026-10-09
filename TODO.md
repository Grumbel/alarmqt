# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** notification styles, Delete/Cancel, Edit/Undo/Redo
- **Next bundle NNN:** 041

## Status

v0.1 feature set is usable. Parser unit tests live under `tests/` (`ctest` / `nix build` check phase).

## Open

- [ ] Recurring alarms, follow-ups: auto-close an unanswered interval
      notification when the next one is due; `until` / count limits
- [ ] Mute / volume for notification sound
- [ ] Shell completions (bash/zsh/fish)

## Done recently

- Notification styles (Settings): Standard (side blinkers), Simple (no
  strips), Fullscreen flash — persisted in QSettings
- Delete confirmation uses Delete/Cancel (not Yes/No)
- Edit menu with Undo/Redo (Ctrl+Z / Ctrl+Shift+Z) for list mutations;
  Edit alarm stays under Edit
- Classic colorful SVG icons on main menu, context menu, and tray menu
- Main menu bar (File / Alarm / Help) with standard actions and About
- Disable / Enable alarms (right-click, Alarm menu, Ctrl+P): paused state
  never fires; status DISABLED; gray row; persists in JSON; restart clears it
- Relative: combined units (`in 1 year 2 months`, `in 2 weeks 3 days`);
  synonyms (`half an hour`, `quarter hour`, `an hour`, `fortnight`); years
- Absolute: `this friday`, `next week` / `next week monday`; `eod` / `end of day`;
  bare hour `at 17`
- Recurring: yearly (`every year on 10-06 at 9:00`); Nth weekday of month
  (`every 2nd tuesday at 18:00`, `every last friday…`); biweekly
  (`every 2 weeks on monday at 9:00`) via weekStride
- Monthly day-of-month; today/tomorrow; bare weekday; weeks/months; date+am/pm;
  noon/midnight; next weekday; unpadded full-date hour

## Handoff

Apply the latest `alarmqt-040.*.bundle` from artifacts (cumulative from `071e920`).
