# AGENTS.md

> Design + implementation guide for automated/codegen agents working on **rawBit** — the tiny, efficient, Windows-only BitTorrent client.

rawBit prioritizes:

* **Small binary size** (a few MB total including libtorrent, not tens)
* **High performance and responsiveness**
* **Extremely small native code surface**
* **Predictable behavior**
* **Zero bloat**

Everything in this document exists to enforce those goals.

---

## 1. Project summary

**rawBit** is a **tiny BitTorrent client for Windows 11**.

Constraints:

* Single native binary, as small as reasonably possible.
* Uses **libtorrent** as the BitTorrent engine.
* All native code is built as **C++**, but written in a **C-style subset** (no “modern C++ soup”).
* UI is **not** native: the real interface is a **TypeScript Web UI** running in the system browser.
* Native binary exposes a **local HTTP/JSON + WebSocket API** and shows only:

  * a **minimal Mica window** with instructions and a button to open the browser UI,
  * a **tray icon** with minimal actions.

Philosophy:

> Minimal moving parts. Minimal dependencies. Maximal control over size and behavior.

---

## 2. Languages and boundaries

### 2.1 Native core

* **Language:** C++ (C++17 or later), but used in a **C-style subset**.
* C++ is used only because **libtorrent is C++**.
* Style rule: think **“C with a bit of RAII and a few small STL containers”**, nothing more.

Core constraints:

* No exceptions.
* No RTTI.
* No heavy STL or template metaprogramming.
* Prefer POD structs, flat arrays, and simple functions.
* Favor data-oriented, straightforward code over abstraction layers.

Agents must keep the code **C-like**, even though it is compiled as C++.

### 2.2 Web UI

* **Language:** TypeScript.
* Built as a **small SPA**: JS bundle + CSS + HTML.
* Served as static files by `rawBit.exe` (e.g. from `/webui/dist/`).
* Runs in the system browser (Edge/Chrome).
* No additional UI frameworks/platforms are allowed beyond that.

Frameworks like React/Vite are acceptable **only if** tuned for a **small bundle**.

---

## 3. C++ usage policy (strict)

### 3.1 Allowed (binary impact negligible)

These may be used freely, but only as needed:

* `std::vector`
* `std::string`
* `std::optional`
* `std::array`
* `std::span` (used minimally)
* Range-based `for` loops
* `constexpr` functions
* `inline` helper functions
* `std::unique_ptr` (no `shared_ptr`)
* RAII wrappers for:

  * file handles,
  * sockets,
  * mutexes (if STL mutex is not used),
* Minimal STL algorithms, e.g.:

  * `std::sort`
  * `std::find`
  * `std::accumulate`

Use these where they **reduce code size and complexity**. If a “nice” abstraction makes the binary bigger, don’t use it.

### 3.2 Forbidden (significant binary bloat)

Agents must never use:

* **Exceptions**

  * Disabled globally (`/EHs- /EHc-`).
  * No `throw`, no `try`, no `catch`.

* **RTTI**

  * Disabled globally (`/GR-`).
  * No `dynamic_cast`, no `typeid`.

* **I/O streams**

  * No `<iostream>`.
  * No `std::cout`, `std::cerr`, `std::stringstream`.

* **Heavy STL containers**

  * `std::map`
  * `std::unordered_map`
  * `std::set`
  * Any other tree/hash-based containers.

* **Heavy template abstractions**

  * `std::function`
  * `std::variant`
  * `std::any`

* **Coroutines** (`co_await` and friends).

* **Boost** (outside libtorrent internals).

* **Filesystem library**

  * No `<filesystem>`; use Win32 API instead.

* **Smart pointers other than `unique_ptr`**.

* **Large metaprogramming/template magic**.

Rule of thumb: if a feature noticeably increases the EXE size, it is forbidden.

---

## 4. Build requirements

Always compile with something equivalent to:

```txt
/O2 /GL /Gy /Gw /Zc:inline
/EHs- /EHc-         # No exceptions
/GR-                # No RTTI
/MT or /MTd         # Static CRT for smallest distribution footprint
/link /OPT:REF /OPT:ICF
```

No extra runtimes are allowed (no .NET, JVM, Python, Node, etc.).

---

## 5. Why no separate C wrapper

Originally the design was:

* C core + C++ wrapper around libtorrent.

This adds:

* Extra exported wrapper functions.
* Extra structs to translate between C and C++.
* Extra glue and copying.

For a size-obsessed project, that’s pointless overhead.

New rule:

* **All native code is C++**, and it calls libtorrent directly.
* Internally we still keep **C-like APIs** (flat structs and functions), but there is **no artificial C/C++ boundary**.

Agents must **not** reintroduce a C-only core with a C++ wrapper **unless** there is a proven, measured size win (very unlikely).

---

## 6. Core design constraints

### 6.1 Binary size

* No Qt, GTK, Electron, WebView2, WinUI, .NET, or any GUI toolkit.
* No embedded browser; use the existing Edge/Chrome.
* No big frameworks for HTTP or JSON. Use:

  * a tiny embedded HTTP server (or minimal hand-rolled one),
  * a tiny JSON serializer/deserializer (or stringify manually where practical).

Always ask: **“Can this be done with less code?”**
If a dependency pulls in tens of thousands of lines, it’s suspect by default.

### 6.2 Responsiveness

* No blocking disk/network calls on the **GUI thread**.
* Torrent engine and HTTP handling run on worker threads.
* Idle CPU usage must be very low:

  * Avoid busy loops and high-frequency timers.
  * Prefer periodic polling (≈ **250–1000 ms**) plus event-driven signaling.

### 6.3 Simplicity

* Avoid abstraction towers.
* Prefer:

  * flat data structures,
  * direct function calls,
  * narrow interfaces.
* Configuration surface is small and explicit:

  * simple config file or hardcoded defaults.

---

## 7. What agents may and may not change

**Allowed:**

* Engine logic (torrent/session management).
* Embedded HTTP server and API routing.
* Win32 launcher window + tray icon behavior.
* Web UI (TypeScript, layout, API usage).
* Libtorrent configuration and integration (within reason).

**Must not:**

* Introduce a second GUI stack.
* Add any runtime that pulls in a large framework (.NET, JVM, Python, Node, Qt, GTK, WebView2, Electron, etc.).
* Add heavy HTTP/JSON libraries or header-only “monstrosities”.
* Introduce cross-platform abstraction layers.
* Change build system away from MSVC/CMake.
* Fork libtorrent unless explicitly required.
* Add background services, drivers, or OS hooks.

Any change that increases EXE size significantly must be justified and is usually rejected.

---

## 8. User interaction model

Agents must preserve this flow:

1. User runs `rawBit.exe`.
2. `rawBit`:

   * Creates libtorrent session.
   * Starts embedded HTTP server on `127.0.0.1:PORT`.
   * Shows a **small Mica window** with:

     * Short text:

       * “rawBit engine is running.”
       * “Click the button to open the control interface in your browser.”
       * Optionally: list any mandatory config steps.
     * One main button: **“Open rawBit interface”**.
   * Creates a **tray icon** with menu items:

     * “Open interface”
     * Optionally “Pause all”
     * “Quit”
3. Clicking **Open interface** (window or tray):

   * Calls `ShellExecute` with `http://127.0.0.1:PORT/`.
4. The browser loads the Web UI, which communicates via HTTP/JSON + WebSocket.
5. Selecting tray “Quit”:

   * Stops HTTP server.
   * Shuts down libtorrent session cleanly.
   * Exits process.

Do not add extra windows, wizards, or complex native UI.

---

## 9. HTTP + WebSocket API expectations

* Server **binds to `127.0.0.1` only**.

* Use plain HTTP/JSON.

* Provide at least these endpoints:

  * `GET /api/torrents`
  * `POST /api/torrents` (add)
  * `POST /api/torrents/{id}/pause`
  * `POST /api/torrents/{id}/resume`
  * `DELETE /api/torrents/{id}`
  * `GET /api/session`

* Provide a WebSocket endpoint (e.g. `/ws`) for:

  * torrent progress updates,
  * torrent finished events,
  * error notifications.

Web UI assumption: this is the **only** backend API.
Agents must **not** add random extra backends or transports.

---

## 10. Architecture and libtorrent usage

### 10.1 High-level components

rawBit consists of:

1. **Engine (C++ C-style)**

   * Direct use of libtorrent.
   * Flat structs.
   * Simple functions.
   * Minimal STL.

2. **HTTP/JSON + WebSocket server**

   * Small and minimal.
   * No frameworks.
   * No huge header-only libraries.

3. **Win32 launcher window**

   * Mica background.
   * Simple 1-button UI.
   * Tray icon.
   * No GUI toolkit.

4. **Web UI (TypeScript SPA)**

   * Served from `/webui/dist/`.
   * Talks to HTTP/JSON API.
   * Uses WebSocket for live updates.

### 10.2 Libtorrent usage rules

* Libtorrent is used **directly** from C++.

* All interaction is funneled through small, local “engine” modules, e.g.:

  * `engine/session.*`
  * `engine/torrent_list.*`
  * `engine/config.*`

* No scattering libtorrent calls across random files.

Rules for agents:

* Group libtorrent usage into a **small number of translation units**.
* Wrap complex libtorrent types into **simple internal structs** for the rest of the app.
* Catch all libtorrent-specific errors at the boundary and report them as simple status codes / error enums.

Do **not**:

* Pass libtorrent types into HTTP or GUI code.
* Expose libtorrent internals in API responses.
* Use exceptions; all error reporting is via return codes / small structs.

---

## 11. Threading model constraints

Agents must respect this model:

* **GUI thread:**

  * Runs the Win32 message loop only.
  * Handles window + tray.
  * Communicates with engine via non-blocking calls or message/queue.

* **Engine thread(s):**

  * Drive libtorrent:

    * poll alerts,
    * manage torrents,
    * aggregate stats.

* **HTTP server thread(s):**

  * Parse HTTP/WebSocket requests.
  * Call engine functions safely (mutex or queue).

Constraints:

* No heavy thread explosion.
* Keep thread count small and predictable.
* No blocking disk/network calls on GUI thread.
* Avoid busy loops; use timers and event-driven signaling.
* Polling intervals for engine work should be around **250–1000 ms**, not microsecond spam.

---

## 12. Explicit “DO NOT” list

Agents must not:

* Use exceptions as control flow (or at all).
* Use RTTI-dependent features (`dynamic_cast`, `typeid`).
* Bring in big template-heavy or header-only libraries unnecessarily.
* Introduce GUI toolkits or embedded browsers.
* Add background services, drivers, or OS hooks.
* Leak libtorrent implementation details into UI or HTTP layers.
* Expose the HTTP server beyond `127.0.0.1`.
* Add extra runtimes (.NET, JVM, Python, Node, etc.).
* Replace MSVC/CMake with other build systems.
* Inflate the EXE with “nice to have” features that don’t materially improve the core behavior.

If in doubt, **do less**.

---

## 13. Allowed modern conveniences

Within the constraints above, agents **may**:

* Use RAII where it clearly reduces cleanup and error paths.

* Use small STL containers (`vector`, `string`, etc.) instead of ad-hoc manual arrays **when** they don’t bloat the binary.

* Implement minimal JSON config parsing:

  * custom tiny parser, or
  * hand-rolled routines, or
  * very small library.

* Implement a slightly thicker HTTP API **only if** it reduces internal complexity or code size.

* Use a compile-time removable logging macro such as:

```cpp
#ifdef RAWBIT_DEBUG
#define RB_LOG(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define RB_LOG(fmt, ...) do {} while (0)
#endif
```

All of this is allowed only so long as it **doesn’t blow up the binary**.

---

## 14. What good contributions look like

Good contributions:

* Smaller code, same or better behavior.
* Fewer moving parts to achieve the same feature.
* Tighter, clearer libtorrent integration.
* Web UI improvements that cost little in bundle size.
* Better responsiveness with **less** CPU usage.
* Simpler threading and synchronization, not more complex.

Agents should:

* Prefer fewer lines of code over generality.
* Always choose the simpler subsystem.
* Remove unnecessary abstractions.
* Coalesce features instead of branching them.
* Optimize for:

  * binary size,
  * clarity,
  * determinism,
  * maintainability.

Everything else is secondary.


## 15. Read ARCHITECTURE.md for architecture plans!
