# rawBit

*A tiny, efficient BitTorrent client for Windows 11 — with a minimal native footprint and a modern browser UI.*

rawBit is built with one goal:
**provide a clean, fast, stable BitTorrent engine with an extremely small binary and zero UI bloat.**

It uses:

* **C++ (C-style subset)** for a compact, predictable native core
* **libtorrent** for the torrent engine
* **a tiny embedded HTTP/JSON + WebSocket server**
* **a minimal Win32 Mica launcher window**
* **a TypeScript browser UI** for all interaction

Nothing else.
No frameworks.
No heavy runtimes.
No GUI toolkits.
No WebView2, no Qt, no GTK, no Electron.

rawBit does exactly what it should — **no more, no less.**

---

## 🚀 Features

* ⚡ **Fast and lightweight** — single small `.exe` + libtorrent
* 🌐 **Browser UI** — runs in Edge/Chrome locally
* 🔌 **Direct libtorrent integration**
* 🖥️ **Minimal Windows 11 launcher** (Mica window + tray icon)
* 🔒 **Local-only HTTP server** (`127.0.0.1:<port>`)
* 📡 **WebSocket live updates**
* 📁 Add torrents via file or magnet link
* ⏸️ Pause/resume
* 🗑️ Remove torrents
* 📊 Real-time stats

---

## 🧱 Architecture (Short Overview)

```
Browser (React/TS) ←→ HTTP/JSON + WebSocket ←→ rawBit.exe (C++ engine)
                                           ↳ Win32 launcher window + tray
```

### Native core (C++ C-style subset)

* Direct calls to libtorrent
* POD structs
* Small STL only (`vector`, `string`, `optional`, etc.)
* No exceptions, no RTTI, no heavy abstractions

### HTTP/JSON + WebSocket server

* Minimal custom server
* No frameworks
* Serves the SPA + API + WS stream

### Win32 launcher

* Small Mica window
* “Open interface” button
* Tray menu: open / pause all / quit

### Browser UI

* TypeScript
* Small bundle
* Talks **only** to the provided HTTP API

More details:
See **AGENTS.md** and **ARCHITECTURE.md**.

---

## 📦 Build Instructions

### Prerequisites

* **Visual Studio 2022** (MSVC)
* **CMake**
* **Python** (for libtorrent build scripts if needed)
* **Node.js** (for building the Web UI)

### Clone

```
git clone https://github.com/your/repo.git rawbit
cd rawbit
git submodule update --init --recursive
```

### Build libtorrent

Refer to the included `scripts/build_libtorrent.ps1` or your own build setup.

### Build rawBit.exe

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Build WebUI

```
cd webui
npm install
npm run build
```

Output goes to `webui/dist/`.

---

## 🏁 Running rawBit

Just run:

```
rawBit.exe
```

It will:

1. Start a libtorrent session
2. Start a tiny local HTTP server on `127.0.0.1:<port>`
3. Show a small launcher window
4. Click “Open interface” → opens browser UI

---

## 🔒 Security Notes

* HTTP server binds to **localhost only**
* No remote access
* No external services
* No cloud dependency

---

## 🧪 Status

rawBit is under active development.
The core architecture is stable and intentionally minimal.
Features will grow **only** when they maintain the size and clarity goals.

---

## 📝 License

MIT

