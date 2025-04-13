# rawBit

A minimal, native Windows BitTorrent client written in pure C.

Designed for Windows 11.  
Built for small size, speed, and simplicity — like software used to be.

---

## Features

- No external dependencies.
- Single `.exe` file.
- Native Win32 API.
- Custom minimal `.torrent` (bencode) parser.
- Lightweight UI with Windows 11 styling.
- Tray icon support.
- Resizable borderless window.
- Clean UI with instant search/filter.
- Safe multi-threaded architecture.

---

## Design Philosophy

- Stay small.
- Stay efficient.
- Stay pure C.
- No cross-platform overhead.
- Native Windows experience.
- 90s software design mindset — fast, simple, understandable.

---

## Build Instructions

### Prerequisites:

- Visual Studio 2022 or newer.
- Latest Windows 11 SDK.

### Build:

1. Clone this repository.
2. Open `rawBit.sln` in Visual Studio.
3. Build → Release x64.

Result: a single `.exe` in `Release\`.

---

## Project Status

rawBit is under active development.  

### Current Progress:
- GUI shell ready.
- Window layout done.
- Tray icon.
- Window resizing.
- Clean window handling.

### Next:
- `torrent.c` — write minimal `.torrent` (bencode) parser.
- Build search index.
- Start Torrent Engine thread.

---

## Documentation

- [docs/PLAN.md](docs/PLAN.md) — Full development milestones.
- [docs/DESIGN.md](docs/DESIGN.md) — Architecture and design notes.

---

## License

TBD.

---

## Notes

rawBit is experimental.  
Work in progress.  
Expect changes.

---

Project: rawBit  
Goal: Minimal Windows 11 BitTorrent client in pure C.  
Target: Windows 11 only.  
Language: C (no C++)  
Build: Visual Studio 2022  
Style: Native Win32 API, small `.exe`, fast, clean code, no dependencies.  
Philosophy: Software should be fast, small, and efficient.

---
