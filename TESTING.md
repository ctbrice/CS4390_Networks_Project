# CS4390 P2P project — testing guide

This project ships a **central tracker** (`skeleton_tracker.c` → `tracker.exe`) and **peers** (`skeleton_peer.c` → `peer.exe`). Wire formats and behavior are implemented to match the course **Project Specification** PDF (`project-des-CS4390.pdf`): periodic `<REQ LIST>`, tracker `<GET filename.track >` for `.track` metadata, `<createtracker …>` / `<updatetracker …>`, P2P transfers in **1024-byte** chunks, MD5 verification, and tracker-side stale-peer pruning driven by the refresh interval in `clientThreadConfig.cfg`.

Before you submit, open the PDF and spot-check: command spellings, angle-bracket message shapes, LIST/GET framing, and any required timing or file-layout rules your instructor emphasized.

---

## Prerequisites

- **Windows** (paths and Winsock usage match the provided Makefile and scripts).
- **GCC** (MinGW-w64 or equivalent) with `-lws2_32`.
- **GNU make** *optional* — the `Makefile` automates builds; you can compile manually (see below).
- **PowerShell** (for `final_demo/Start-FinalDemo.ps1`).

---

## 1. Clean tree (optional but recommended)

From the repo root:

1. Stop any running peers or tracker: Task Manager → end `peer.exe` / `tracker.exe`, or in PowerShell:
   ```powershell
   Get-Process tracker,peer -ErrorAction SilentlyContinue | Stop-Process -Force
   ```
2. Remove demo folders if present (`peer1` … `peer13`) and runtime `tracker_shared` contents you do not intend to commit. With a normal `.gitignore`, `git status` should only show source changes.

---

## 2. Build tracker and peer

### Option A — `make` (if installed)

```powershell
cd <path-to-repo>
make clean
make all
```

This produces `tracker.exe` and `peer1/peer.exe` … `peer13/peer.exe` (one binary per folder for the demo layout).

### Option B — manual `gcc` (same as Makefile)

```powershell
cd <path-to-repo>
gcc -Wall -Wextra -g -c skeleton_peer.c -o skeleton_peer.o
gcc -Wall -Wextra -g -c skeleton_tracker.c -o skeleton_tracker.o
gcc -Wall -Wextra -g -o tracker.exe skeleton_tracker.o -lws2_32
mkdir peer1, peer2, peer3, peer4, peer5, peer6, peer7, peer8, peer9, peer10, peer11, peer12, peer13 -Force
gcc -Wall -Wextra -g -o peer1/peer.exe skeleton_peer.o -lws2_32
for ($p = 2; $p -le 13; $p++) { Copy-Item peer1\peer.exe "peer$p\peer.exe" -Force }
```

Fix any compiler errors before continuing.

---

## 3. Configuration files

### Repo root (tracker + template for peers)

- **`clientThreadConfig.cfg`** — line 1: tracker IP; line 2: tracker port; line 3: refresh interval (seconds). Lines may include `#` comments.
- **`serverThreadConfig.cfg`** — used by the peer skeleton for listen port / shared folder (see file format in `skeleton_peer.c`).

The **tracker** reads tracker port and refresh interval from **its working directory’s** `clientThreadConfig.cfg` (normally the repo root).

Each **peer** reads configs from **its own** working directory when you start it from `peerN\`.

---

## 4. Smoke test — tracker + one peer (auto mode)

1. Ensure `tracker_shared` contains at least the committed seed `Socket_Tutorial.pdf.track` (or run `git checkout -- tracker_shared` if you removed tracked files).
2. Copy `Socket_Tutorial.pdf` into `peer1\sharedFolder\` (same folder name the `.track` file references).
3. Terminal 1 — start tracker from repo root:
   ```powershell
   cd <path-to-repo>
   .\tracker.exe
   ```
4. Terminal 2 — start one peer in `peer1` with **auto** registration / LIST / downloads:
   ```powershell
   cd <path-to-repo>\peer1
   ..\peer1\peer.exe auto
   ```
   (Adjust path if your `peer.exe` lives elsewhere.)

**What to look for**

- Console lines showing tracker connection, `[refresh]` LIST activity, and no repeated `GET … bad response or MD5 mismatch`.
- If the tutorial file is present and trackers match, you should see registration and normal refresh logs.

Stop with **Ctrl+C** in the peer window, then stop the tracker.

---

## 5. Manual protocol checks (single peer, repo root)

From repo root, with tracker running:

```powershell
.\peer1\peer.exe list
```

You should see `<REP LIST …>` lines and `<REP LIST END>`.

```powershell
.\peer1\peer.exe get Socket_Tutorial.pdf.track
```

You should see `<REP GET BEGIN>`, the `.track` body lines, then `<REP GET END …>`.

`createtracker` / `updatetracker` subcommands exist for debugging (see `main()` in `skeleton_peer.c`).

---

## 6. Stress test — `Start-FinalDemo.ps1`

This script builds a **13-peer** layout (`peer1`…`peer13`) under the repo root, wipes `peer1`/`peer2` seeds’ folders (then re-seeds), optionally clears `tracker_shared`, starts the tracker, starts seeds and leechers on the spec timeline (6 leechers at 30 s, 5 at 90 s), then kills the two seeds and prints `Peer1 terminated` / `Peer2 terminated`.

From repo root (after **§2** build):

```powershell
Get-Process tracker,peer -ErrorAction SilentlyContinue | Stop-Process -Force
powershell -NoProfile -ExecutionPolicy Bypass -File .\final_demo\Start-FinalDemo.ps1 -LargeFileMiB 1
```

- **`-LargeFileMiB 1`** — ~1 MiB random `demo_large.bin` on peer2 (faster than default 8).
- **`-NoCleanTracker`** — keep existing `tracker_shared` between runs (optional).

**What to look for**

- No `bad response or MD5 mismatch` on tracker GETs.
- `demo_small.txt` should complete on multiple leechers; `demo_large.bin` may show `download incomplete` on some peers after seeds are **killed on purpose** — that is a limitation of the demo timeline, not necessarily a protocol bug.
- After the script returns, stop stragglers:
  ```powershell
  Get-Process tracker,peer -ErrorAction SilentlyContinue | Stop-Process -Force
  ```

**Important:** Do **not** redirect the demo to a single log file with `*> file.log` unless peers are started with stdout detached; child processes can inherit the handle and keep the pipeline open.

---

## 7. Files to commit vs ignore

- **Commit:** `skeleton_peer.c`, `skeleton_tracker.c`, `Makefile`, `README.md`, PDF, tutorial `cache/` / `tracker_shared/` seeds, `final_demo/Start-FinalDemo.ps1`, `TESTING.md`, `.gitignore`, config templates as required by the course.
- **Do not commit:** `*.o`, `*.exe`, generated `peer1`…`peer13` trees, extra files under `tracker_shared/` produced at runtime (see `.gitignore`).

---

## 8. If something fails

| Symptom | Things to verify |
|--------|-------------------|
| Peer cannot connect | Tracker running; IP/port in `clientThreadConfig.cfg`; firewall |
| `bad response or MD5 mismatch` on GET | Tracker and peer built from current sources; `tracker_shared` not corrupted; line endings in `.track` files (UTF-8 LF from the demo script) |
| LIST empty | Seeds ran `createtracker`; `tracker_shared\*.track` exist |
| P2P stalls | Listen ports in `serverThreadConfig.cfg`; `PEER_ID` / working directory correct per peer |
| `download incomplete` after demo | Expected for some leechers when seeds are killed mid-transfer; rerun with larger timeline or don’t kill seeds |

---

## 9. Final checklist before submission

1. `gcc` (or `make`) builds with **no errors** (warnings are acceptable but review them).
2. PDF vs code: message formats, ports, chunk size **1024**, MD5 usage, tracker file layout.
3. `Start-FinalDemo.ps1 -LargeFileMiB 1` completes without tracker GET parse errors.
4. `git status` is clean aside from intentional source/docs; no binaries or `peer*` demo trees tracked.
