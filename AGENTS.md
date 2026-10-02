# AlarmQt – Agent Notes

Notes for automated agents (Grok) working on this repo. Humans contributing
normally can ignore the **Grok delivery** section and use ordinary git push /
PR workflow against GitHub.

## Project

Simple keyboard-friendly system-tray alarm / reminder for Linux.

- **Stack:** C++20, Qt6 Widgets (no QML), CMake, Nix flake
- **Repo:** https://github.com/Grumbel/alarmqt.git
- **License:** GPL-3.0-or-later (REUSE-compliant)
- **Author:** Ingo Ruhnke <grumbel@gmail.com>

## Architecture

| Piece | Role |
|-------|------|
| `Alarm` | POD + JSON (`command`, `label`, `triggerUtc`, flags) |
| `AlarmManager` | Parse, persist (`~/.local/share/alarmqt/alarms.json`), 1 Hz tick, signals |
| `MainWindow` | Clock, table, input; tray; edit/restart/clear DONE |
| `NotificationDialog` | Always-on-top dialog; side blinkers + sound; Ack / Snooze |
| `SingleInstance` | `QLocalServer` – second process forwards CLI and exits |

- Alarms are stored as absolute UTC; UI shows local time.
- `command` = time expression (`in 5m`, `at 15:10`); `label` = optional note.
- Relative / absolute / note parsing lives in `AlarmManager::parse`.
- Acknowledged alarms stay as **DONE** until removed or cleared.

## Versioning

- Sole source of truth: top-level `VERSION` (e.g. `0.1.0-dev` on the main branch).
- Dev builds (Nix): append `.{revCount}+g{shortRev}` when `VERSION` contains `-dev`.
- Release: drop `-dev`, commit, tag `vX.Y.Z`, then bump to the next `*-dev`.
- Full string is `ALARMQT_VERSION` / `QApplication::applicationVersion()`.

## CLI

```
alarmqt                  # raise
alarmqt "in 5m"          # add
alarmqt "in 5m (kitchen)"
alarmqt "at 15:10"
alarmqt --list
alarmqt --quit
alarmqt --raise
```

## Build

```bash
nix build
nix run . -- "in 1m"
alarmqt --version
```

## Standing rules (agents)

- Prefer correct design over quick hacks; no silent feature removal.
- New/changed sources: short SPDX headers (`GPL-3.0-or-later`).
- Build and experiment outside the session artifacts tree when possible.
- Read `TODO.md` at session start; keep tip / next bundle NNN accurate.

---

## Grok delivery only (git bundles)

**Normal contributors do not use this.** Pushes and pull requests go through
GitHub as usual. The `.bundle` workflow exists only for Grok sessions that
cannot push to the remote and must hand commits back as catch-up packages.

### Rules

- Deliverables are **git bundle** files only (no patch files).
- Each bundle is **cumulative** from the work-line **base** commit through
  `HEAD` (not a delta since the previous bundle).
- Bundles must fast-forward cleanly onto that base; never invent parallel history.
- Author: `Ingo Ruhnke <grumbel@gmail.com>`
- Every commit: trailer `Co-authored-by: Grok <grok@x.ai>`
- Naming: `alarmqt-{NNN}.{rev}-short-slug-{shortBase}.bundle`
  - `{shortBase}` is fixed for the stack (first checkout base of the line).
  - Never reuse an earlier `{NNN}`; supersede intermediates; keep only the tip bundle.
- Place finished bundles where the user can download them; state the full path.
- Checkout/build in a temp directory; copy only the tip bundle into artifacts.

### Create / apply

```bash
# create (from work-line base .. HEAD)
git bundle create alarmqt-NNN.M-slug-$(git rev-parse --short BASE).bundle BASE..HEAD

# apply (user / next session)
git pull /path/to/alarmqt-NNN.M-slug-BASE.bundle HEAD
```
