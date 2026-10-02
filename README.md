# AlarmQt

Simple system-tray alarm / reminder app for Linux (NixOS-friendly).

## Features

- **Relative & absolute alarms**
  - `in 5m`, `in 2h30m`, `in 1d`
  - `at 15:10`, `at 2026-10-03 09:00`
  - optional note: `in 5m (kitchen)`, `in 10m "tea"`
- **Timezone-aware** (uses local timezone by default; stores absolute UTC instants)
- **Persistent** – alarms survive restarts (JSON in `~/.local/share/alarmqt/alarms.json`)
- **Repeats until acknowledged** – always-on-top flashing dialog + tray balloon every 30 s until Ack / Snooze
- **System tray** – icon shows next alarm countdown; left-click toggles window, right-click menu
- **Single-instance** – starting the binary again focuses the existing window / sends CLI commands
- **Keyboard-driven**
  - `Ctrl+N` / focus line edit → type alarm → Enter
  - `Delete` / `Ctrl+D` remove selected
  - `Space` / Enter on triggered dialog = Acknowledge
  - `Esc` hides window to tray
- **Countdown** shown for every alarm and in the tray tooltip
- **Current time** displayed prominently
- Finished alarms stay in the list as **DONE** (edit/remove manually)
- **Edit** via button, Ctrl+E, or double-click
- Classic **QWidget** UI (no QML)

## Build (Nix)

```bash
nix build
./result/bin/alarmqt
# or
nix run
```

## CLI

```bash
alarmqt                    # start / raise
alarmqt "in 5m"            # add relative alarm
alarmqt "at 15:10"         # add absolute alarm
alarmqt --list
alarmqt --quit
```

## Dependencies

- Qt6 (Core, Gui, Widgets, Network for local socket)
- CMake ≥ 3.16
- C++20

## Desktop integration

Installed files (via CMake / Nix):

- `share/applications/alarmqt.desktop`
- `share/icons/hicolor/scalable/apps/alarmqt.svg`
- `share/metainfo/alarmqt.metainfo.xml`

The app sets `QGuiApplication::setDesktopFileName("alarmqt")` so the tray and window match the desktop entry (icon, notifications, single-window hints).

## License

GPL-3.0-or-later
