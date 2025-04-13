
# rawBit — Final Milestone Plan  
*Pure C BitTorrent Client for Windows 11*

**Core Philosophy:**  
- **Correctness & Simplicity first**, optimization second.  
- **Minimalistic**, clean C architecture—no external dependencies.  
- **Clear thread ownership** and safe inter-thread communication.  
- **Scalable architecture**, easy future enhancements.  
- **Modern Windows 11 UI**, responsive UX.

---

## Architecture & Threading

### Thread Responsibilities:

| Thread                | Ownership                                              | Notes                                 |
|-----------------------|--------------------------------------------------------|---------------------------------------|
| **Main Thread**       | UI, Search, User interaction, global app state         | Never blocked; only UI operations     |
| **Torrent Engine Thread** | Torrent logic, networking, peers, trackers, state machine | Non-blocking socket event loop        |
| **Disk IO Thread**    | File read/write operations (optional; add if blocking) | Added if necessary                    |
| **Hash Thread**       | SHA1 piece verification (optional; add if blocking)    | Added if necessary                    |

### Cross-thread Communication:
- Threads communicate **only via messages** (`PostMessage`) or simple event queues.
- **No shared mutable state**—clearly defined data ownership.

---

## Module Structure & Ownership:

| Module               | Responsibility                                      | Thread Ownership          |
|----------------------|-----------------------------------------------------|---------------------------|
| `main.c`             | Application entry, message loop, startup            | Main                      |
| `ui.c`               | Window creation, UI layout, status updates          | Main                      |
| `platform.c/h`       | OS abstraction, theming, system calls               | Main                      |
| `win11.c`            | Windows 11 specific calls                           | Main                      |
| `torrent.c/h`        | Torrent state, metadata, torrent operations         | Torrent Engine            |
| `peer.c/h`           | BitTorrent peer-protocol implementation             | Torrent Engine            |
| `tracker.c/h`        | Tracker requests/responses                          | Torrent Engine            |
| `torrent_engine.c/h` | Thread management, network event loop, torrent updates | Torrent Engine            |
| `search.c/h`         | Search index, filtering logic, UX updates           | Main                      |
| `diskio.c/h`         | Buffered disk I/O operations (optional)             | Disk IO Thread            |
| `hash.c/h`           | SHA1 hashing of pieces (optional)                   | Hash Thread               |

---

## Finalized Milestones & Deliverables

### Phase 1: Application Skeleton
- **Create project structure:** directories, empty files.
- **Main app:** `main.c` with WinMain, message loop.
- **Window layout:** minimal `ui.c` with empty ListView, top search box, bottom status/buttons.
- **OS Abstraction:** `platform.h/c` + `win11.c`.

---

### Phase 2: Torrent Parsing & Loading
- **Implement `.torrent` parser:** minimal Bencode parsing in `torrent.c`.
- **Data structure:** populate Torrent structs.
- **ListView population:** Load torrents from disk, display names and basic info.

---

### Phase 3: Search System Implementation
- **Index torrents and files:** store in-memory index (`search.c`).
- **Instant UI filtering:** Linear search initially.
- **Prepare for scalable search:** clean API for future Trie upgrade.

---

### Phase 4: Torrent Engine Thread & Networking
- **Torrent engine thread creation:** implement `torrent_engine.c`.
- **Event loop:** Non-blocking sockets (`select()` or `WSAEventSelect()`).
- **Basic tracker communication:** HTTP tracker handling (`tracker.c`).
- **Stub peer connections:** initial peer connection handshake structure (`peer.c`).

---

### Phase 5: BitTorrent Protocol Implementation
- **Full peer handshake & messaging:** piece requesting/sending.
- **Piece management:** download/upload, basic choking/unchoking.
- **Piece verification:** simple inline hash verification initially.

---

### Phase 6: UX & Modern UI Polishing
- **Theme:** Light/Dark modes (`PlatformApplyTheme`).
- **Window handling:** draggable window without title bar.
- **UX refinement:** context menus (right-click torrent), basic UI interactions.
- **Drag & Drop:** Torrent file drag/drop integration.

---

### Phase 7: OS Integration (File/Magnet Handling)
- **File associations:** `.torrent` and magnet link OS registrations.
- **Command-line handling:** open torrents/magnets via args.
- **Automatic torrent addition:** seamless UX.

---

### Phase 8: Disk & Hashing Threading Enhancements (Optional, if needed)
- **Disk IO thread:** buffered reads/writes to prevent UI stalls (`diskio.c`).
- **Hash thread:** offload piece hashing if blocking UI or network loop (`hash.c`).

---

### Phase 9: Performance Review & Optimization
- **Profiling:** RAM, CPU, search performance under heavy load.
- **Search upgrade:** if linear search slow, implement Trie or Radix Tree.
- **Optimize networking:** peer handling, efficient piece selection.

---

### Phase 10: Final Polish & Release Readiness
- **Branding:** application icon, consistent visual polish.
- **Tray integration:** minimize to tray, tray icon status (optional).
- **About & versioning:** final user-facing info.
- **Clean documentation & build:** ready for first release.

---

## Iterative Review & Future-proofing Notes
- **Maintain clear API boundaries** to allow easy upgrades.
- **Avoid premature optimizations** but **choose good structures early**.
- **Keep modules small**, well-defined responsibility per file.
- **Regular profiling**, user experience checks at each stage.

---

## Additional Considerations (Future Features - Post v1)
- Optional RSS feed support.
- Optional remote/web management.
- Optional advanced torrent management (labels, scheduler, etc.).
- Advanced user filtering (inside archives).
