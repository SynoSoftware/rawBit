# AGENTS.md

> Design + implementation guide for automated/codegen agents working on **rawBit**.

## 1. Project summary

**rawBit** is a **tiny BitTorrent client for Windows 11**.

Constraints:

- Single native binary, as small as reasonably possible (≈ a few MB, not tens).
- Uses **libtorrent** as the BitTorrent engine.
- All native code is built as **C++**, but written in a **C-style subset** to avoid C++ bloat.
- UI is **not** native: the real interface is a **TypeScript Web UI** running in the system browser.
- Native binary exposes a **local HTTP/JSON + WebSocket API** and shows only:
  - a **minimal Mica window** with instructions and a button to open the browser UI
  - a **tray icon** with minimal actions.

The philosophy:  
**Minimal moving parts. Minimal dependencies. Maximum control over size and behavior.**

---

## 2. Languages and constraints

### 2.1 Native side

- **Language:** C++ (C++17 or later), but:
  - No exceptions (`/EHs-` or equivalent, and do not throw/catch).
  - No RTTI (`/GR-` or equivalent).
  - No heavy STL usage:
    - Avoid `<iostream>`, `<string>` (or keep usage extremely limited).
    - Avoid `std::map`, `std::unordered_map`, `std::shared_ptr`, etc.
  - Prefer POD structs, flat arrays, and simple functions.
  - Think “**C with RAII where it clearly reduces code**”, nothing more.

Agents must keep the code **C-like**. C++ is only there because libtorrent is C++.

### 2.2 Web UI

- **Language:** TypeScript.
- Compiled to a small JS bundle + CSS + HTML.
- No frameworks with huge runtime cost unless they compile down lean (React/Vite is acceptable if tuned for a small bundle).

---

## 3. Why no separate C wrapper

Originally: C core + C++ wrapper around libtorrent.

This adds:

- Extra exported wrapper functions.
- Extra structs to translate between C and C++.
- Extra glue and copies.

For a size-obsessed project, that’s unnecessary.

New rule:

- **All native code is C++**, and it calls libtorrent directly.
- We still keep C-like APIs internally (flat structs and functions), but no artificial language boundary.

Agents must **not** reintroduce a C-only core with a C++ wrapper unless there is a proven, measured size win (unlikely).

---

## 4. Core design constraints

### 4.1 Binary size

- No Qt, GTK, Electron, WebView2, WinUI, .NET, or any GUI toolkit.
- No embedded browser; use the existing Edge/Chrome.
- No big frameworks for HTTP or JSON. Use:
  - a tiny embedded HTTP server (or minimal hand-rolled one),
  - a tiny JSON serializer/deserializer (or stringify manually where practical).

Always ask: *Can this be done with less code?*  
If a dependency pulls tens of thousands of lines, it’s suspect.

### 4.2 Responsiveness

- No blocking disk/network calls on the GUI thread.
- Torrent engine and HTTP handling run on worker threads.
- Idle CPU usage must be very low:
  - Avoid busy loops and high-frequency timers.
  - Prefer periodic polling (e.g. 250–1000 ms) plus event-driven signaling.

### 4.3 Simplicity

- Avoid abstraction towers.
- Prefer:
  - flat data structures,
  - direct function calls,
  - narrow interfaces.
- Configuration surface is small and explicit (simple config file or hardcoded defaults).

---

## 5. What agents may and may not change

**Allowed:**

- Engine logic (torrent/session management).
- Embedded HTTP server and API routing.
- Win32 launcher window + tray icon behavior.
- Web UI (TypeScript, layout, API usage).
- Libtorrent configuration and integration (within reason).

**Must not:**

- Introduce a second GUI stack.
- Add a dependency that pulls in a large runtime without very strong justification.
- Fork libtorrent unless explicitly required.

---

## 6. User interaction model

1. User runs `rawBit.exe`.
2. `rawBit`:
   - Creates libtorrent session.
   - Starts embedded HTTP server on `127.0.0.1:PORT`.
   - Shows a **small Mica window**:
     - Short text:
       - “rawBit engine is running.”
       - “Click the button to open the control interface in your browser.”
       - Optionally: list any mandatory config steps.
     - Button: **“Open rawBit interface”**.
   - Creates a **tray icon** with:
     - “Open interface”
     - Optionally “Pause all”
     - “Quit”
3. Clicking **Open interface**:
   - Calls `ShellExecute` with `http://127.0.0.1:PORT/`.
4. The browser loads the Web UI, which communicates via HTTP/JSON + WebSocket.
5. Closing the tray menu “Quit”:
   - Stops HTTP server.
   - Shuts down libtorrent session cleanly.
   - Exits process.

Agents must preserve this flow.

---

## 7. HTTP API expectations

- Bind to `127.0.0.1` only.
- Use plain HTTP/JSON.
- Provide at least:

  - `GET /api/torrents`
  - `POST /api/torrents` (add)
  - `POST /api/torrents/{id}/pause`
  - `POST /api/torrents/{id}/resume`
  - `DELETE /api/torrents/{id}`
  - `GET /api/session`

- Provide a WebSocket endpoint (e.g. `/ws`) for:
  - torrent progress updates,
  - torrent finished events,
  - error notifications.

Web UI assumptions: this is the **only** backend API.

---

## 8. Libtorrent usage rules

- Libtorrent is used **directly** from C++.
- All interaction is funneled through small, local “engine” modules, e.g.:

  - `engine/session.*`
  - `engine/torrent_list.*`
  - `engine/config.*`

- No scattering libtorrent calls across random files.

Rules for agents:

- Group libtorrent usage into a small number of translation units.
- Wrap complex libtorrent types into simple internal structs for the rest of the app.
- Catch all libtorrent-specific errors at the boundary and report them as simple status codes / error enums.

Do not:

- Pass libtorrent types into HTTP or GUI code.
- Use exceptions; all error reporting is via return codes / small structs.

---

## 9. Threading model constraints

Agents must respect:

- GUI thread:
  - Win32 message loop only.
  - Communicates with engine via non-blocking calls or message/queue.
- Engine thread(s):
  - Drive libtorrent:
    - poll alerts,
    - manage torrents,
    - aggregate stats.
- HTTP server thread(s):
  - Parse HTTP/WebSocket.
  - Call engine functions safely (mutex or queue).

No heavy thread explosion.  
Keep thread count small and predictable.

---

## 10. Explicit “DO NOT” list

Agents must not:

- Use exceptions as control flow.
- Use RTTI-dependent features (`dynamic_cast`, `typeid`).
- Bring in big template-heavy libraries unnecessarily.
- Introduce GUI toolkits or embedded browsers.
- Add background services, drivers, or OS hooks.
- Leak implementation details of libtorrent into UI or HTTP layers.

---

## 11. What good contributions look like

- Smaller code, same or better behavior.
- Fewer moving parts to achieve the same feature.
- Tighter, clearer libtorrent integration.
- Web UI improvements that cost little in bundle size.
- Better responsiveness with less CPU usage.

Always aligned to this principle:

> Minimal code, minimal dependencies, maximal control.
