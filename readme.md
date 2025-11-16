# rawBit

rawBit is a tiny, Windows-only BitTorrent client.  
The native binary is only responsible for the libtorrent engine, the HTTP/JSON + WebSocket API, and a minimal Win32 launcher window with Mica styling + tray icon. Everything else happens in the browser UI.

Goals:

* keep the executable extremely small,
* keep performance predictable,
* expose a single, well defined API to the TypeScript front-end,
* avoid every form of UI bloat.

The design constraints are documented in `AGENTS.md` and expanded in `ARCHITECTURE.md`.

---

## Features

* **Single native binary** compiled with MSVC – no Electron, WebView2, GTK, Qt, .NET, or scripting runtimes.
* **Direct libtorrent session** wrapped in a tiny C-style engine module (no exceptions, no RTTI, minimal STL).
* **Embedded HTTP/JSON + WebSocket server** (Mongoose) bound to `127.0.0.1:<port>`.
* **Browser UI (TypeScript SPA)** served from `webui/dist/` – add torrents, pause/resume/remove, monitor stats, live updates via WebSocket.
* **Win32 launcher window + tray icon** with one “Open interface” button and quick commands.
* **Local-only security posture** – nothing leaves the machine, no remote control surface is exposed.

---

## Architecture (short overview)

```
Browser (TypeScript SPA) <-- HTTP/JSON + WebSocket --> rawBit.exe
                                                        |
                                                        +-- Engine (libtorrent session)
                                                        |
                                                        +-- Win32 launcher window + tray icon
```

* `src/engine` – owns the libtorrent session and torrent registry (C++ written in a C-style subset).
* `src/net` – embedded HTTP server, API routing, JSON serialization, WebSocket broadcast channel.
* `src/platform/win32` – launcher window, tray icon, and ShellExecute link to the browser UI.
* `webui/` – TypeScript SPA compiled to static assets that are served by the native binary.

---

## Build instructions

### Prerequisites

* Visual Studio 2022 with MSVC toolset v143.
* CMake (if you prefer CMake builds) or the provided Visual Studio solution.
* Python + Boost to build libtorrent (see `scripts/build_libtorrent.ps1`).
* Node.js ≥ 18 for the Web UI.

### Clone + submodules

```powershell
git clone https://github.com/your/repo.git rawBit
cd rawBit
git submodule update --init --recursive
```

### Build libtorrent

*Use the provided helper or your own build pipeline.*

```powershell
pwsh .\scripts\build_libtorrent.ps1 -Configuration Release
```

This produces `external/libtorrent-build/<Configuration>/torrent-rasterbar.lib`, which the Visual Studio project links against.

### Build rawBit.exe

Open `rawBit.sln` in Visual Studio and build the `rawBit` project (Debug or Release).  
The project already includes the correct include/library paths and linker flags.

### Build the Web UI

```
cd webui
npm install
npm run build
```

This compiles `src/app.ts` to `dist/app.js` and copies the static HTML/CSS assets into `webui/dist/`. The HTTP server serves that directory automatically.

---

## Running rawBit

```
rawBit.exe
```

What happens:

1. The engine starts a libtorrent session and worker thread.
2. The embedded HTTP server binds to `http://127.0.0.1:<port>/` (default `32145`).
3. A Mica launcher window appears with the **Open rawBit interface** button.
4. Clicking the button (or the tray icon entry) launches the default browser pointed at the local UI.
5. The browser UI communicates exclusively via the documented HTTP/JSON endpoints and `/ws`.

Use the tray icon to reopen the UI or quit the process. Selecting **Quit** shuts down the HTTP server and the engine cleanly.

---

## Security notes

* The HTTP server only listens on `127.0.0.1`.
* No remote control or NAT exceptions are opened.
* No credentials or telemetry are sent anywhere – everything remains on the local machine.

---

## Status

rawBit is intentionally small and opinionated.  
Contributions are welcome only when they keep to the constraints in `AGENTS.md` – no new runtimes, no giant template libraries, no gratuitous features. The priority order is **binary size → determinism → clarity**.

---

## License

MIT
