# Tiny Guy

A ESP32 smart display that displays a 'tiny guy' working when your AI agents are working.

## Features

- Displays a 'tiny guy' working when your AI agents are working.
- Shows usage left for your AI agents.
- Support for Claude Code, Codex and Cursor.

## Codex usage

The simulator reads the signed-in user's Codex quota from the local Codex CLI
using the official app-server protocol. Install `codex`, run `codex login`, and
start Tiny Guy normally; the usage page refreshes automatically once a minute.
Credentials remain managed by Codex and are not stored by Tiny Guy.

## Claude usage

The simulator reuses the local Claude Code OAuth login to fetch subscription
usage. Install `claude`, run `claude auth login`, and make sure `curl` is
installed. The orange `5h` and `week` bars show the percentage remaining in the
current 5-hour and 7-day limits. Tiny Guy reads the existing credential for
each request and does not store a copy. The usage
endpoint is used by Claude Code but is not a documented public Anthropic API
and may change in a future release.

## Cursor usage

The simulator also reads the signed-in Cursor desktop app's monthly included
usage from Cursor's dashboard. White `cursor` and `other` bars show the
remaining Cursor Models and third-party Other Models pools respectively. Sign
in to Cursor locally and make sure the `sqlite3` and `curl` commands are
installed. Tiny Guy reads Cursor's existing local session for each request; it
does not store a copy of the credentials. This dashboard endpoint is not a
documented public Cursor API and may change in a future Cursor release.

## Product specs

For this project, I used an ESP32-S3 development board with the following specs:

- 32-bit LX7 dual-core processor
- Type-C port
- 1.8 inch
- 368 x 448 pixels
- AMOLED panel
- PCF85063 RTC chip
- QMI8658 6-axis IMU
- SH8601 display driver
- FT3168 / FT6146 touch chip
- AXP2101 PMU

## Arch Linux setup

```sh
sudo pacman -S --needed base-devel cmake ninja sdl2 gdb
git submodule update --init --recursive
```

In VS Code, install Microsoft's **C/C++** extension (`ms-vscode.cpptools`). Then:

- Press `Ctrl+Shift+B` to configure and build.
- Run **LVGL: Debug emulator** from the Run and Debug panel (or press `F5`).
- Alternatively run the **LVGL: run emulator** task without a debugger.

The starter UI lives in `src/ui.c`. Keep portable UI code there and keep the
SDL setup in `src/main.c`; this makes it straightforward to reuse the UI from
the ESP32-S3 application.

The simulator defaults to a 368 x 448 display. Change `SDL_HOR_RES` and
`SDL_VER_RES` in `config/lv_drv_conf.h` to match the AMOLED panel exactly.
