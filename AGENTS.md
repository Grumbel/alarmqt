# AlarmQt – Agent Notes

## Project

Simple keyboard-friendly system-tray alarm / reminder for Linux.

- **Stack:** C++20, Qt6 Widgets (no QML), CMake, Nix flake
- **Repo:** https://github.com/Grumbel/alarmqt.git
- **License:** GPL-3.0-or-later (REUSE-compliant)
- **Author:** Ingo Ruhnke <grumbel@gmail.com>

## Architecture

| Piece | Role |
|-------|------|
| `Alarm` | POD + JSON (id, label, triggerUtc, flags) |
| `AlarmManager` | Parse, persist (`~/.local/share/alarmqt/alarms.json`), 1 Hz tick, signals |
| `MainWindow` | List + input line + status; owns tray |
| `NotificationDialog` | Always-on-top flashing dialog; Ack / Snooze |
| `SingleInstance` | `QLocalServer` – second process forwards CLI and exits |

Alarms are stored as absolute UTC. UI shows local time. Relative (`in 5m`) and absolute (`at 15:10`) parsing lives in `AlarmManager::parse`.

`command` is the time expression; `label` is an optional note. Acknowledged alarms stay as DONE until removed or cleared.

## Standing rules

- Prefer correct design over quick hacks.
- Do not remove features without discussion.
- New code: short SPDX headers (`GPL-3.0-or-later`).
- Bundles: cumulative from work-line base, author + `Co-authored-by: Grok <grok@x.ai>`.
- Build/test in `/tmp` (or similar); only bundles and final deliverables into artifacts.

## CLI contract

```
alarmqt                  # raise
alarmqt "in 5m"          # add
alarmqt "at 15:10"       # add
alarmqt --list
alarmqt --quit
```

## Build

```bash
nix build
nix run . -- "in 1m"
```
