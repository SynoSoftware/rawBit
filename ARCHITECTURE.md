
# ARCHITECTURE.md

## 1. Overview

**rawBit** is a **Windows 11 BitTorrent client** focused on:

- Small native binary (≈ a few MB).
- Minimal UI footprint.
- Using libtorrent as the engine with direct C++ integration.
- A browser-based Web UI (TypeScript → JS) for all real interaction.

Architecture:

```text
+--------------------+         HTTP/JSON + WebSocket        +-----------------------+
|  Browser (Edge)    | <-----------------------------------> | Embedded HTTP Server  |
|  Web UI (TS → JS)  |                                       | inside rawBit.exe     |
+--------------------+                                       +-----------+-----------+
                                                                       |
                                                                       v
                                                         +-------------+-------------+
                                                         |      Engine (C++ C-style) |
                                                         |  - Libtorrent session     |
                                                         |  - Torrent registry       |
                                                         |  - Config & stats         |
                                                         +-------------+-------------+
                                                                       ^
                                                                       |
                                 +-------------------------------------+-------------------------+
                                 |    Win32 Mica window + tray icon (launcher & minimal status) |
                                 +--------------------------------------------------------------+
````

---

## 2. Components

### 2.1 `rawBit.exe`

Single native process, built as C++ (C-style).

Responsibilities:

* Create and manage a libtorrent session.
* Manage torrents (add, pause, resume, remove, query status).
* Expose a **local-only** HTTP/JSON + WebSocket API.
* Show a minimal **Mica window** with:

  * a short message,
  * a button to open the Web UI.
* Provide a **tray icon** for quick access and exit.

Submodules:

1. **Engine core (`engine/`)**

   * `engine_session`:

     * Owns libtorrent `session`.
     * Applies configuration (limits, features flags).
   * `engine_torrents`:

     * Stores torrent entries (ID, name, handles, basic state).
     * Provides simple functions:

       * `add_torrent(...)`
       * `pause_torrent(id)`
       * `resume_torrent(id)`
       * `remove_torrent(id)`
       * `get_torrent_status(id, &out_status)`
   * `engine_stats`:

     * Aggregates global stats:

       * download/upload rate
       * totals
       * counts (number of torrents, peers).

   All of this is direct C++ integrating with libtorrent, but using plain structs and functions.

2. **HTTP server + API (`net/`)**

   * Minimal HTTP listener bound to `127.0.0.1:PORT`.
   * Routes:

     * `/` → serve `index.html` (from Web UI dist folder).
     * `/assets/...` → static files (`app.js`, `app.css`, etc.).
     * `/api/...` → JSON API endpoints.
     * `/ws` → WebSocket endpoint.
   * API handlers call the engine functions synchronously or via a lightweight queue (depending on locking strategy).

3. **Event/notification layer**

   * Periodically polls libtorrent alerts from `engine_session`.
   * Translates alerts to internal events:

     * torrent added
     * torrent finished
     * error
     * tracker update
   * Pushes events out via WebSocket to connected Web UI clients.

4. **Win32 launcher (`platform/win32/`)**

   * Creates a small **Mica** window:

     * Title: `rawBit`
     * Static text: engine running, instruction text (and any mandatory config hints).
     * Button: **“Open interface in browser”**.
   * Creates a tray icon with context menu:

     * “Open interface”
     * Optional: “Pause all”
     * “Quit”
   * `Open interface`:

     * Calls `ShellExecute` with `http://127.0.0.1:PORT/`.
   * `Quit`:

     * Signals engine to shut down:

       * Stop HTTP server.
       * Close libtorrent session.
       * Exit cleanly.

---

## 3. Native language choices

Even though the codebase is compiled as C++, design follows:

* plain structs and enums,
* plain functions,
* minimal RAII where it clearly simplifies resource handling (sockets, file handles, etc.),
* no exceptions, no RTTI, minimal STL usage.

This allows:

* direct use of libtorrent,
* minimal binary bloat,
* simple mental model similar to C.

No separate C wrapper layer exists.

---

## 4. Threading model

Target:

* **GUI thread** (Win32):

  * Runs message loop.
  * Does not call heavy engine operations directly.
  * Interacts via small, non-blocking calls or posts signals.

* **Engine thread**:

  * Owns the libtorrent session.
  * Performs:

    * alert polling.
    * torrent state updates.
    * periodic stat aggregation.
  * Processes commands from:

    * HTTP server (add/pause/resume/remove),
    * GUI (pause all/shutdown).

* **HTTP server thread(s)**:

  * Accepts and handles HTTP/WebSocket connections.
  * Parses requests, writes responses.
  * Calls engine methods via:

    * direct calls with mutex protection, or
    * a command queue processed by engine thread.

* **Libtorrent internal threads**:

  * Disk IO, networking, etc. (managed internally by libtorrent).

Invariants:

* Engine thread is the single writer of torrent and session state.
* HTTP and GUI threads may read aggregated status snapshots under light locking.
* No long blocking operations on GUI thread.

---

## 5. Libtorrent configuration

Centralized in an engine configuration module:

* At session creation:

  * Apply default parameters:

    * limits on connections,
    * enable/disable DHT, uTP, encryption as decided,
    * rate limits (optional).
  * Only a small, documented subset of libtorrent options is used.

Rationale:

* All session behavior is tuned from one place.
* Changes to libtorrent configuration are easy to audit and reason about.

---

## 6. HTTP API (high level)

Core endpoints:

* `GET /api/torrents`

  * Returns:

    * `id`
    * `name`
    * `progress`
    * `download_rate`
    * `upload_rate`
    * `state`
    * `num_peers`
* `POST /api/torrents`

  * Add torrent.
  * Body:

    * `{ "path": "C:\\path\\file.torrent" }` or `{ "magnet": "magnet:?..." }`
  * Returns: `{ "id": "<id>" }`
* `POST /api/torrents/{id}/pause`
* `POST /api/torrents/{id}/resume`
* `DELETE /api/torrents/{id}`
* `GET /api/session`

  * Global stats: rates, totals, counts, etc.

Events via WebSocket `/ws`:

* `torrent_updated`
* `torrent_finished`
* `torrent_error`
* `session_update` (optional for global stats)

The API is intentionally narrow; all UI functionality must sit on top of this.

---

## 7. Startup and shutdown

**Startup:**

1. `main()`:

   * Load configuration.
   * Initialize libtorrent session and engine thread.
   * Start HTTP server.
   * Initialize Win32, create Mica window + tray icon.
2. Launcher shows:

   * Short instructions.
   * “Open interface” button.
3. User clicks “Open interface”:

   * Browser opens `http://127.0.0.1:PORT/`.
   * Web UI loads and starts polling `/api/...` and connecting to `/ws`.

**Shutdown:**

1. User selects “Quit” from tray or closes the main window.
2. GUI signals engine:

   * HTTP server stops accepting new connections.
   * Existing connections are given a short timeout to finish.
   * Engine stops torrents cleanly and shuts down libtorrent session.
3. Main thread waits for engine/HTTP threads to exit.
4. Process terminates.

Goal: no zombie threads, no stuck sockets, no dirty shutdown.

---

## 8. Files and directories

Suggested layout:

```text
/ rawbit
  /src
    /engine        # libtorrent integration, session/torrents/stats (C++ C-style)
    /net           # HTTP server, routing, WebSocket, JSON handling
    /platform
      /win32       # Mica window, tray icon, ShellExecute, etc.
  /webui
    /src           # TS source for SPA
    /dist          # Built static assets (index.html, app.js, app.css, etc.)
  AGENTS.md
  ARCHITECTURE.md
```

---

## 9. Extension points

Future features:

* More torrent metadata:

  * extend internal status structs,
  * extend JSON returned by `/api/torrents`.
* Settings UI:

  * add `/api/settings` endpoints.
* Per-torrent limits:

  * add engine calls for speed/connection limits.

Any extension:

* must reuse the existing engine and HTTP infrastructure,
* must not introduce new protocols or runtimes,
* must keep the binary small and behavior predictable.

The core structure—**native engine + HTTP API + browser UI + minimal Win32 launcher**—is fixed and should not be overturned.

```

::contentReference[oaicite:0]{index=0}
```
