# rawBit

A minimal, native Windows BitTorrent client written in pure C.

Designed for Windows 11.  
Built for small size, speed, and simplicity — like software used to be.

---

## Features

- No external dependencies.
- Single `.exe` file.
- Native Win32 API.
- Custom minimal `.torrent` parser.
- Lightweight UI with Windows 11 styling.
- Tray icon support.
- Resizable borderless window.

---

## Build Instructions

### Prerequisites:

- Visual Studio 2022 or newer.
- Windows SDK (latest).

### Build:

1. Clone the repository.
2. Open `rawBit.sln` in Visual Studio.
3. Build → Release x64.

That's it — a single `.exe` will be generated.

---

## Goals

- Stay small.
- Stay efficient.
- Stay pure C.
- No cross-platform overhead.
- Native Windows experience.

---

## License

TBD

---

## Notes

rawBit is experimental and under heavy development.  
Expect rapid changes.

---

Project: rawBit  
Goal: Minimal Windows 11 BitTorrent client in pure C.  
Style: Native Win32 API, single .exe, small size, clean code, no dependencies.  
Status: GUI shell ready, windowing logic done, starting torrent.c for .torrent parser.
Language: C (no C++)  
Build: Visual Studio 2022  
Target: Windows 11 only  
Philosophy: 90s software design — fast, small, efficient.
"Write torrent.c — minimal .torrent (bencode) parser."


---

Project: rawBit  
Goal: Minimal Windows 11 BitTorrent client written in pure C.  
Target: Windows 11 only.  
Style: Native Win32 API, single .exe, small size, fast, clean code, no dependencies.  
Build: Visual Studio 2022  
Current status: GUI shell working, tray icon, resizing, clean window handling.  
Next: Start torrent.c — minimal .torrent (bencode) parser, pure C, zero malloc abuse.  
Philosophy: 90s-style software — fast, small, efficient, understandable.

