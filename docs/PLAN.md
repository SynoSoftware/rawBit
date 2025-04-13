
# rawBit — Final Milestone Plan  
*Pure C BitTorrent Client for Windows 11*

**Core Philosophy:**  
- Correctness & Simplicity first, optimization second.  
- Minimalistic, clean C architecture — no external dependencies.  
- Clear thread ownership and safe inter-thread communication.  
- Scalable architecture, easy future enhancements.  
- Modern Windows 11 UI, responsive UX.  
- Page-based UI system — single window, multiple logical pages.

---

## Architecture & Threading

### Thread Responsibilities:

| Thread                   | Ownership                                               | Notes                                 |
|--------------------------|---------------------------------------------------------|---------------------------------------|
| Main Thread              | UI, Search, User interaction, global app state, Page Management | Never blocked; only UI operations     |
| Torrent Engine Thread    | Torrent logic, networking, peers, trackers, state machine| Non-blocking socket event loop        |
| Disk IO Thread           | File read/write operations (optional; add if blocking)  | Added if necessary                    |
| Hash Thread              | SHA1 piece verification (optional; add if blocking)     | Added if necessary                    |

### Cross-thread Communication:
- Threads communicate only via messages (`PostMessage`) or simple event queues.
- No shared mutable state — clearly defined data ownership.

---

## Module Structure & Ownership:

| Module                    | Responsibility                                          | Thread Ownership          |
|---------------------------|---------------------------------------------------------|---------------------------|
| main.c                    | Application entry, message loop, startup                | Main                      |
| ui.c                      | Window creation, main UI setup, resize handling         | Main                      |
| platform.c/h              | OS abstraction, theming, system calls                   | Main                      |
| win11.c                   | Windows 11 specific calls                               | Main                      |
| torrent.c/h               | Torrent state, metadata, torrent operations             | Torrent Engine            |
| peer.c/h                  | BitTorrent peer-protocol implementation                 | Torrent Engine            |
| tracker.c/h               | Tracker requests/responses                              | Torrent Engine            |
| torrent_engine.c/h        | Thread management, network event loop, torrent updates  | Torrent Engine            |
| search.c/h                | Search index, filtering logic, UX updates               | Main                      |
| diskio.c/h                | Buffered disk I/O operations (optional)                 | Disk IO Thread            |
| hash.c/h                  | SHA1 hashing of pieces (optional)                       | Hash Thread               |
| ui/page.h                 | Page struct definition, page interface                  | Main                      |
| ui/pagemanager.c          | Page switching logic                                    | Main                      |
| ui/torrentlistpage.c      | Main view with torrent list, search                     | Main                      |
| ui/settingspage.c         | App settings UI and logic                               | Main                      |
| ui/aboutpage.c            | About info, version info                                | Main                      |
| ui/torrentdetailspage.c   | Detailed torrent file info, progress (optional)         | Main                      |

---

## Finalized Milestones & Deliverables

### Phase 1: Application Skeleton
- Create project structure with `src/` and `docs/` folders.
- Setup `main.c` with WinMain, message loop.
- Layout window in `ui.c` with empty ListView, search box, status/buttons.
- OS Abstraction layer (`platform.h/c`, `win11.c`).

### Phase 2: Torrent Parsing & Loading
- `.torrent` parser (Bencode) in `torrent.c`.
- Load torrents from disk.
- Populate torrent data in the UI ListView.

### Phase 3: Search System Implementation
- Build `search.c` for indexing torrent/file names.
- Instant UI filtering via search box.
- API prepared for scalable Trie upgrade.

### Phase 4: Page System Implementation
- Create page API (`ui/page.h`).
- Implement page switching (`ui/pagemanager.c`).
- Implement `torrentlistpage.c`, `settingspage.c`, `aboutpage.c`.
- Optional `torrentdetailspage.c` for per-torrent details.

### Phase 5: Torrent Engine Thread & Networking
- Implement `torrent_engine.c`.
- Non-blocking event loop (`select()` or `WSAEventSelect()`).
- Tracker handling (`tracker.c`).
- Peer handshake and initial communication (`peer.c`).

### Phase 6: BitTorrent Protocol Implementation
- Full peer protocol (request/send/choke messages).
- Piece management and verification.

### Phase 7: UX & Modern UI Polishing
- Theme integration (`platform.c`).
- Window handling (drag without title bar).
- UX enhancements (context menus, drag-drop).

### Phase 8: OS Integration (File/Magnet Handling)
- File association (`.torrent`, magnet links).
- CLI argument parsing.

### Phase 9: Disk IO & Hashing Enhancements
- Buffered file IO (`diskio.c` optional).
- Piece hashing offloaded (`hash.c` optional).

### Phase 10: Performance Review & Final Polish
- Profiling and optimization.
- Search optimization (Trie).
- UI and branding final polish (tray, icon).

---

## Future Considerations (Post v1)
- Archive content search.
- Remote management API.
- Plugin architecture.
- Advanced torrent management.

---

## Iterative Review & Future-proofing Notes
- **Maintain clear API boundaries** to allow easy upgrades.
- **Avoid premature optimizations** but **choose good structures early**.
- **Keep modules small**, well-defined responsibility per file.
- **Regular profiling**, user experience checks at each stage.

