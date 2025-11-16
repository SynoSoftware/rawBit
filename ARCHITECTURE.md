# ARCHITECTURE.md

rawBit is a **tiny Windows-only BitTorrent client** with:

* C++ engine using a **C-style subset** of modern C++
* direct **libtorrent** integration (no C wrapper)
* tiny embedded **HTTP/JSON + WebSocket** server
* minimal **Win32 Mica launcher window** + tray icon
* real UI in the system browser (TypeScript SPA)

Goal: **do the job perfectly, stay extremely small, keep full control.**
For C++ rules, STL whitelist and build flags, see **AGENTS.md**.

---

## 1. System overview

```text
+--------------------+         HTTP/JSON + WebSocket      +-----------------------+
|  Browser (Edge)    | <---------------------------------> | Embedded HTTP Server  |
|  Web UI (TS → JS)  |                                      | inside rawBit.exe     |
+--------------------+                                      +-----------+-----------+
                                                                       |
                                                                       v
                                                         +-------------+-------------+
                                                         |   Engine (C++ C-style)    |
                                                         |  - libtorrent session    |
                                                         |  - torrent registry      |
                                                         |  - stats & config        |
                                                         +-------------+-------------+
                                                                       ^
                                                                       |
                                 +-------------------------------------+-------------------------+
                                 |   Win32 Mica window + tray icon (launcher & minimal status)  |
                                 +--------------------------------------------------------------+
```

---

## 2. Components

### 2.1 `rawBit.exe` (single native process)

Responsibilities:

* Create and manage the **libtorrent session**.
* Manage torrents (add, pause, resume, remove, query status).
* Expose a **local-only** HTTP/JSON + WebSocket API.
* Show a minimal **Mica window** with:

  * short text (engine running, how to open UI),
  * one button to open the Web UI.
* Provide a **tray icon** for quick access and exit.

Submodules (names indicative):

1. **Engine core (`src/engine/`)**

   * `engine_session`

     * Owns libtorrent `session`.
     * Applies configuration (limits, feature flags).
     * Polls alerts.
   * `engine_torrents`

     * Stores torrent entries (ID, name, libtorrent handle, state).
     * Provides simple functions:

       * `add_torrent(...)`
       * `pause_torrent(id)`
       * `resume_torrent(id)`
       * `remove_torrent(id)`
       * `get_torrent_status(id, &out_status)` (fills a small POD struct)
   * `engine_stats`

     * Aggregates global stats:

       * download/upload rates,
       * totals,
       * counts (torrents, peers).

   All of this is direct C++ integration with libtorrent, but via **plain structs and functions**.

2. **HTTP server + API (`src/net/`)**

   * Minimal HTTP listener bound to `127.0.0.1:PORT`.
   * Routes:

     * `/` → serve `index.html` (Web UI).
     * `/assets/...` → static files (`app.js`, `app.css`, etc.).
     * `/api/...` → JSON API endpoints.
     * `/ws` → WebSocket endpoint.
   * API handlers call engine functions directly or via a small command queue (depending on locking model).

3. **Event / notification layer**

   * Runs on the engine thread (or a small helper inside `engine_session`).
   * Periodically polls libtorrent alerts.
   * Translates alerts to internal events:

     * torrent added / removed
     * torrent finished
     * error
     * tracker updates
   * Pushes events out via WebSocket to connected Web UI clients.

4. **Win32 launcher (`src/platform/win32/`)**

   * Creates a small **Mica** window:

     * Title: `rawBit`
     * Static text: engine running, “open in browser” instructions, optional mandatory config hints.
     * Button: **“Open rawBit interface”**.
   * Creates a tray icon with context menu:

     * “Open interface”
     * Optional: “Pause all”
     * “Quit”
   * “Open interface”:

     * Calls `ShellExecute` with `http://127.0.0.1:PORT/`.
   * “Quit”:

     * Signals engine to shut down:

       * Stop HTTP server.
       * Close libtorrent session.
       * Exit cleanly.

---

## 3. Native language choices

Even though the codebase is compiled as C++:

* Design uses:

  * plain structs and enums,
  * plain functions,
  * minimal RAII where it clearly simplifies resource handling (sockets, file handles, etc.).
* No exceptions, no RTTI, minimal STL — see **AGENTS.md** for the detailed usage policy.
* **No separate C wrapper** layer around libtorrent.

Result:

* Direct use of libtorrent.
* Minimal binary bloat.
* Simple mental model similar to C.

---

## 4. Threading model

Target threads:

* **GUI thread (Win32)**

  * Runs the message loop.
  * Owns the Mica window and tray icon.
  * Does not perform heavy engine operations.
  * Interacts with engine via:

    * small, non-blocking calls, or
    * posted messages / command queue.

* **Engine thread**

  * Owns the libtorrent `session`.
  * Performs:

    * alert polling,
    * torrent state updates,
    * periodic stats aggregation.
  * Processes commands from:

    * HTTP server (add/pause/resume/remove),
    * GUI (pause all / shutdown).

* **HTTP server thread(s)**

  * Accept and handle HTTP/WebSocket connections.
  * Parse requests, write responses.
  * Call engine methods via:

    * direct calls under a light mutex, or
    * a command queue processed by the engine thread.

* **Libtorrent internal threads**

  * Disk I/O, networking, etc. managed internally by libtorrent.

Invariants:

* Engine thread is the **single writer** of torrent and session state.
* HTTP and GUI threads may read **aggregated snapshots** under light locking.
* No long blocking operations on the GUI thread.
* No thread explosion: keep thread count small and predictable.
* No busy loops or high-frequency timers; typical engine polling interval **250–1000 ms**.

---

## 5. Libtorrent configuration

Centralized in an engine configuration module (e.g. `engine_config`):

* At session creation:

  * Apply default parameters:

    * connection limits,
    * DHT / uTP / encryption settings (as decided),
    * rate limits (optional).
* Only a **small, documented subset** of libtorrent options is used.
* All configuration lives in one place so behavior is easy to audit.

Other code **must not** tweak libtorrent configuration directly.

---

## 6. HTTP API (high level)

Server requirements:

* Bind to `127.0.0.1` only.
* Use plain HTTP/JSON.
* Narrow, stable API that the Web UI fully relies on.

Minimum endpoints:

* `GET /api/torrents`
  Returns a list of torrents with fields like:

  * `id`
  * `name`
  * `progress` (0..1 or %)
  * `download_rate`
  * `upload_rate`
  * `state`
  * `num_peers`

* `POST /api/torrents`
  Add torrent. Body:

  * `{ "path": "C:\\path\\file.torrent" }` **or**
  * `{ "magnet": "magnet:?..." }`
    Returns:
  * `{ "id": "<id>" }`

* `POST /api/torrents/{id}/pause`

* `POST /api/torrents/{id}/resume`

* `DELETE /api/torrents/{id}`

* `GET /api/session`
  Returns global stats:

  * rates,
  * totals,
  * counts, etc.

WebSocket at `/ws`:

* Emits events such as:

  * `torrent_updated`
  * `torrent_finished`
  * `torrent_error`
  * optional `session_update` for global stats

The Web UI **must** use only this backend. No extra native protocols.

---

## 7. Startup and shutdown

### 7.1 Startup

1. `main()`:

   * Load configuration (if any).
   * Initialize libtorrent session and engine state.
   * Start engine thread.
   * Start HTTP server.
   * Initialize Win32, create Mica window and tray icon.
2. Launcher window shows:

   * Short instruction text.
   * “Open rawBit interface” button.
3. User clicks “Open interface”:

   * Browser opens `http://127.0.0.1:PORT/`.
   * Web UI loads, calls `/api/...` and connects to `/ws`.

### 7.2 Shutdown

1. User selects “Quit” from tray or closes the main window.
2. GUI signals engine:

   * HTTP server stops accepting new connections.
   * Existing connections get a short timeout to finish.
   * Engine stops torrents cleanly and shuts down libtorrent session.
3. Main thread waits for engine and HTTP threads to exit.
4. Process terminates.

Goal: no zombie threads, no stuck sockets, no dirty shutdown.

---

## 8. Files and directories

Suggested layout:

```text
/ rawbit
  /src
    /engine        # libtorrent integration, session/torrents/stats (C++ C-style)
      engine_session.*
      engine_torrents.*
      engine_stats.*
      engine_config.*
    /net           # HTTP server, routing, WebSocket, JSON handling
      http_server.*
      http_routes.*
      ws_server.*
    /platform
      /win32       # Mica window, tray icon, ShellExecute, etc.
        win_main.*
        win_tray.*
  /webui
    /src           # TS source for SPA
    /dist          # Built static assets (index.html, app.js, app.css, etc.)
  AGENTS.md
  ARCHITECTURE.md
```

Exact names can vary, but **separation of concerns must remain**:

* `engine/` owns libtorrent and torrent state.
* `net/` owns HTTP/WS, JSON, and routing.
* `platform/win32/` owns Win32 boilerplate.
* `webui/` is pure TypeScript/HTML/CSS, with no native code.

---

## 9. Extension points

Future extensions should:

* Reuse the existing engine + HTTP + Web UI structure.
* Keep the API narrow and JSON-based.
* Stay within the C++ subset and build rules defined in **AGENTS.md**.

Examples:

* More torrent metadata:

  * extend internal status structs,
  * extend JSON returned by `/api/torrents`.
* Settings UI:

  * add `/api/settings` endpoints,
  * store config in a small INI or tiny JSON file.
* Per-torrent limits:

  * engine calls for speed/connection limits,
  * matching API and UI controls.

Forbidden:

* New runtimes (no .NET, JVM, Python, Node, Qt, GTK, WebView2, Electron).
* Heavy libraries or huge dependency trees.
* New GUI stacks.
* Alternative frontends that bypass the HTTP/JSON + WebSocket API.
* Any change that significantly increases EXE size for marginal gain.

---

## 10. Architecture philosophy

The core structure is fixed:

> **Native engine + HTTP API + browser UI + minimal Win32 launcher.**

* C++ with only the size-safe parts (see AGENTS.md).
* Direct libtorrent integration.
* Win32 launcher only; real UI is in the browser.
* Tiny HTTP server; narrow JSON API; one WebSocket endpoint.
* No bloat, no unnecessary layers.

rawBit does exactly what it should — **no more, no less — with the smallest possible footprint.**
