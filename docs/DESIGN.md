
# rawBit — Design Document

## Project Architecture Overview

rawBit is a minimalist BitTorrent client written in pure C for Windows 11.  
It focuses on small executable size, fast execution, clean UX, and zero dependencies.

---

## High-Level Architecture

```
 Main Thread (UI, AppState, Search)
        |
        v
 Torrent Engine Thread (Peers, Trackers, Protocol)
        |
        +-- Disk IO Thread (Optional - File Read/Write)
        |
        +-- Hash Thread (Optional - Piece Hashing)
```

- The Main Thread owns the UI and interacts with the user.
- Torrent Engine Thread manages torrent state, networking, peers, and trackers.
- Disk IO and Hash threads are added only if performance bottlenecks require them.

---

## Thread Ownership Rules

| Thread | Responsibilities | Data Ownership |
|--------|------------------|----------------|
| Main Thread | UI, AppState, Search | UI state, Search Index |
| Torrent Engine | Torrent State, Networking | Torrent structs, Peer structs |
| Disk IO Thread | File IO (Optional) | File buffers during IO |
| Hash Thread | SHA1 Verification (Optional) | Piece buffer during hash |

---

## Cross-thread Communication

- Threads communicate via `PostMessage()` or internal message queues.
- No shared mutable data across threads.
- Ownership is strict — only one thread modifies a given piece of data.

---

## Module Structure

| Module | Purpose |
|--------|---------|
| `main.c` | Application entry point, message loop |
| `ui.c` | UI layout, ListView, user input handling |
| `platform.c` | OS abstraction layer |
| `win11.c` | Windows 11 specific API usage |
| `torrent.c` | Torrent state, metadata, progress |
| `peer.c` | Peer-to-peer protocol handling |
| `tracker.c` | Tracker requests and responses |
| `torrent_engine.c` | Manages torrent state machine, peers, event loop |
| `search.c` | Search and filtering system |
| `diskio.c` | Buffered file IO (optional) |
| `hash.c` | Piece SHA1 hash verification (optional) |
| `debug.c` | Debug logging helpers |

---

## UI Layout Overview

```
+----------------------------------------------+
| [ Filter torrents...                      ] |
+----------------------------------------------+
| Torrent ListView (Name | Status | Progress) |
+----------------------------------------------+
| Status Line: Speed | Peers | Disk Activity   |
+----------------------------------------------+
| [Start] [Stop] [Remove] [Settings]          |
+----------------------------------------------+
```

- No titlebar for a clean modern look.
- Responsive layout on resize.
- Optional dark/light theming with accent color.

---

## Search System Design

- Search system indexes torrent names and all file names within torrents.
- In-memory search index for instant filtering.
- Future upgrade path to Trie or Radix Tree for large datasets.
- Designed for responsiveness and scalability.

---

## Future Considerations

- Archive content indexing (ZIP/RAR/7Z) in background.
- Advanced UI features (custom controls, owner-draw ListView).
- Remote control API (optional).
- Plugin architecture (if rawBit expands).

---

This document defines the design principles and architecture of rawBit v1.
All implementation should follow these guidelines for clarity, scalability, and long-term maintainability.
