# AMS Sortie

A small floating dial that lives on your Windows desktop. Drop files or folders on it,
pick a petal, and they are moved (or copied) into that folder.

**AMS** · Made by **Rehman Ahmed** · WhatsApp / phone **+92 306 0706989**

---

## What was broken in the first version

These are the reports that shaped this rebuild, and what was actually wrong:

| Report | Root cause | Fix |
| --- | --- | --- |
| "It crashes when I move a lot of data / long transfers" | The whole move ran in a **blocking loop on the GUI thread** (`DropZoneWidget::moveFiles` → `FileMover::move`). While copying, the window could not repaint, Windows marked it *Not responding*, and the process was killed. Directory copies were also **recursive** (`copyRecursive`), so a deep tree could blow the stack, and the size check walked the whole tree **twice** before doing anything. | A dedicated **worker thread** (`TransferWorker` + `TransferManager`) does the job. Copies are **iterative** (`QDirIterator`), chunked at 1 MiB with **cancellation checks between chunks**, partial files are written as `*.sortie-part` and only renamed once verified, and nothing is ever deleted before the copy is verified. |
| "There is no progress bar like Windows shows, I need progress and total-vs-transferred size" | There was no progress concept at all; the dial just went quiet. | A real **transfer window** (`TransferDialog`): progress bar with percentage, *transferred of total* size, speed, time left, file counts, the current item, plus **Cancel** and *Run in background*. The dial itself also draws a live progress ring with `x of y` bytes. |
| "Improve the layout" | Petals existed in the menu only — the dial was a plain circle with two lines of text. | The petals are now **drawn as a ring** on the dial (colour-coded, numbered, hover names, click to open/move). The dial shows brand, state and progress, with an `+` badge for adding folders, and it all scales smoothly from 120 px to 460 px. |
| "Floating icon should stretch, use Windows stretch anchors" | The window was a fixed square: no resize grips, no snapping, and it could be lost off-screen. | Shared `FloatingWindow` base: **8 stretch grips** (corners + edges), wheel resize, double-click reset, **edge snapping**, and **anchoring** — the dial and the WhatsApp bubble remember which screen edge they are glued to and follow it when the resolution or taskbar changes. |
| "Add branding AMS / Rehman Ahmed / phone" | Not present anywhere. | Brand applied to the dial, tray menu, About card, window icon, file properties, installer artwork, Start Menu entry and Apps & Features. |
| "Smart floating WhatsApp contact icon" | Not present. | `WhatsAppBubble`: a real Windows icon-bubble you can drag, **stretch**, snap, pulse, and click to open a chat with `+92 306 0706989`. |

Other hardening that came out of the review:

* Config moved from *next to the .exe* (which is read-only under `Program Files`) to
  `%APPDATA%\AMS\AMS Sortie\sortie.json`, saved atomically with `QSaveFile`, with migration
  of the old `petals_config.json`.
* **Long paths** (> 260 chars) get the `\\?\` prefix on Windows instead of failing silently.
* **Free-space check** before a job starts, with a clear "need X, only Y free on D:" message.
* **Undo last move** for the previous job.
* **Single instance**: launching again raises the running dial instead of starting a second copy.
* **Rotating log file** in `%LOCALAPPDATA%\AMS\AMS Sortie\logs` plus a Windows crash filter, so a
  failure leaves evidence instead of vanishing.
* **Start with Windows** (HKCU Run key, no admin needed).
* Ctrl-drop to **copy** instead of moving; Esc / number keys drive the folder picker.

---

## Install (end users)

Download the latest **`AMS Sortie-1.0.0-win64.msi`** from the release page and double-click it.

* Installs **per machine** into `C:\Program Files\AMS\Sortie` (admin prompt once).
* Adds a **Start Menu** shortcut, an **Apps & Features** entry with the AMS icon, and a clean
  uninstaller. Installing a newer MSI upgrades the old version in place.
* The Qt runtime is bundled — nothing else needs to be installed.
* `AMS Sortie-1.0.0-win64.exe` (NSIS) is an alternative setup with a Desktop-shortcut option.
* The `.zip` is a portable copy: no installation, just run `SortieApp.exe`.

## Use

* **Drag** the dial anywhere. **Stretch** it from any corner or edge. **Wheel** = resize,
  **double-click** the grip = default size. It snaps to screen edges and stays anchored there.
* **Drop** files or folders on it → a picker lists your folders (or click a petal directly).
  Hold **Ctrl** while dropping to copy instead of move. **Esc** cancels.
* While a job runs the dial shows a progress ring — click it, or use the tray, to open the
  full transfer window.
* **Click a petal** with nothing pending to open that folder.
* **Right-click** the dial: add / manage folders, undo, WhatsApp bubble, always-on-top,
  lock position, snapping, start with Windows, logs, About, quit.
* The **WhatsApp bubble**: drag, stretch, or click to chat with Rehman Ahmed.

## Build from source

Requirements: **Qt 6.4+** (Core, Gui, Widgets, Svg), CMake 3.21+, a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows one-click: run **`BUILD_SORTIE.bat`** (downloads Qt/CMake into `_toolchain` if needed,
builds, deploys with `windeployqt`, and builds the MSI when WiX is present).

### Installers from GitHub Actions

`.github/workflows/build-installers.yml` builds everything on every push and publishes on tags:

* `linux-compile-check` — compiles the whole code base with GCC/Qt 6 (fast error feedback).
* `windows-installers` — MSVC + Qt 6.8, stages the app with `windeployqt`, then
  * **MSI** via CPack + **WiX v4** (`CPACK_WIX_*`, per-machine, Start Menu, upgrade GUID),
  * **setup.exe** via CPack + **NSIS** (Desktop shortcut option),
  * **portable .zip**.

Artifacts: *Actions → Build installers → ams-sortie-windows-x64*. To publish a release:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Layout

```
CMakeLists.txt            build + install + CPack (WiX / NSIS)
app.rc                    icon + version resource
assets/app.manifest       PerMonitorV2 DPI, long-path awareness
assets/icons              brand icon (.svg/.png/.ico), WhatsApp glyph, folder icons
assets/installer          MSI banner / dialog art, NSIS header
installer/license.rtf     licence shown by the installers
src/                      application code (see below)
tools/make_assets.py      regenerates every raster brand asset
.github/workflows         CI that produces the installers
```

Application code:

| File | Responsibility |
| --- | --- |
| `main.cpp` | app metadata, single instance, tray, wiring |
| `OrbWidget` | the dial: petals, progress ring, drop handling, menus |
| `PetalPicker` | non-blocking "move to" chooser |
| `TransferWorker` / `TransferManager` | threaded, cancellable, verified move engine |
| `TransferDialog` | Windows-style progress window |
| `FloatingWindow` | drag, stretch grips, snapping, screen anchors |
| `WhatsAppBubble` | floating WhatsApp contact icon |
| `Config` | atomic per-user settings + v1 migration |
| `Theme` / `Brand` / `Format` | design tokens, publisher details, size formatting |
| `Logging`, `SingleInstance`, `Autostart`, `AboutDialog` | support infrastructure |

## Contact

**Rehman Ahmed (AMS)**
WhatsApp / phone: **+92 306 0706989** · Email: rehmanahmed92@yandex.com

Copyright (C) 2026 Rehman Ahmed (AMS). All rights reserved.
