
# rawBit — Design Document

## Project Architecture Overview

rawBit is a minimalist BitTorrent client written in pure C for Windows 11.

It focuses on:
- Small executable size.
- Fast execution.
- Clean, native UX.
- No external dependencies.

---

## High-Level Architecture

```
 Main Thread (UI, AppState, Search, Page Management)
        |
        v
 Torrent Engine Thread (Peers, Trackers, Protocol State)
        |
        +-- Disk IO Thread (Optional - File Read/Write)
        |
        +-- Hash Thread (Optional - Piece Hashing)
```

---

## Thread Ownership Rules

| Thread                 | Responsibilities                                   | Data Ownership |
|------------------------|---------------------------------------------------|----------------|
| Main Thread            | UI, AppState, Search, Page Management             | UI State, Search Index |
| Torrent Engine Thread  | Torrent Logic, Peers, Trackers, Protocol          | Torrent Structs, Peer Structs |
| Disk IO Thread         | File I/O (Optional)                               | File Buffers |
| Hash Thread            | Piece SHA1 Verification (Optional)                | Piece Buffers |

---

## Cross-thread Communication

- All threads communicate via `PostMessage()` or event queues.
- No shared mutable state across threads.
- Each thread fully owns its working data.

---

## UI Page System Design

rawBit uses a single-window page-based UI system for a modern and clean UX.

### Implemented Pages:

| Page Module                 | Purpose                                    |
|----------------------------|--------------------------------------------|
| `torrentlistpage.c`        | Main torrent list view, search/filtering.  |
| `settingspage.c`           | Application settings (paths, limits).      |
| `aboutpage.c`              | About information, version details.        |
| `torrentdetailspage.c` (Optional) | Per-torrent detailed info view.   |

Page switching is handled via a clean API in `pagemanager.c`.

---

## Module Structure

| Module                    | Responsibility                                          |
|---------------------------|---------------------------------------------------------|
| `main.c`                  | App entry, message loop.                                |
| `ui.c`                    | Window creation, layout, page management.               |
| `platform.c/h`            | OS abstraction.                                         |
| `win11.c`                 | Windows 11 specific features.                          |
| `torrent.c/h`             | Torrent handling and metadata.                         |
| `peer.c/h`                | Peer communication and protocol.                       |
| `tracker.c/h`             | Tracker communication.                                 |
| `torrent_engine.c/h`      | Networking thread event loop.                          |
| `search.c/h`              | Search and filtering logic.                            |
| `diskio.c/h`              | Background file IO (Optional).                         |
| `hash.c/h`                | Background SHA1 hashing (Optional).                    |
| `ui/page.h`               | Page API definition.                                   |
| `ui/pagemanager.c`        | Page switching logic.                                  |
| `ui/torrentlistpage.c`    | Main page showing torrent list.                       |
| `ui/settingspage.c`       | Settings page.                                        |
| `ui/aboutpage.c`          | About page.                                           |
| `ui/torrentdetailspage.c` | Torrent detail page (Optional).                       |

---

## Future Considerations

- Archive content indexing (ZIP, RAR, 7Z).
- Advanced search across torrent files and archives.
- Remote control API.
- Plugin architecture for modular features.

---

This document defines rawBit v1 design principles, architecture, and module ownership for clarity and long-term maintainability.
