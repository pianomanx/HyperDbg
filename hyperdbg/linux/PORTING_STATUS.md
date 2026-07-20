# HyperDbg Linux Port — Status & TODO Ledger

This file tracks **what has been changed** for the Linux port and **what is still
stubbed / deferred**, so that when the port compiles end-to-end we have a single
list of the shortcuts that must be revisited before Linux is actually functional.

It complements [`README.md`](README.md) (the contributor how-to). This file is
the *state* of the work; the README is the *method*.

> Status in one line: the userspace library (`libhyperdbg`) compiles file-by-file
> on Linux. Many Windows-only paths are **stubbed to compile+link**, not yet
> implemented. See the TODO ledger below.

---

## Conventions used in this port

Two patterns, applied consistently:

1. **In-body `#ifdef _WIN32`** — for a *few* Windows-only functions inside an
   otherwise shared file. The Windows body stays; the Linux `#else` returns a
   safe default with a `TODO(Linux)` comment.
   Examples: `DebuggerGetNtoskrnlBase`, the `$peb` pseudo-register, the two
   test harnesses in `script-engine-wrapper.cpp`.

2. **Separate `*-linux.cpp` file + CMake swap** — for a *whole* translation unit
   that is entirely Windows-specific with only a few public entry points. The
   original Windows `.cpp` is left 100% untouched; a Linux stub file implements
   the same public functions; `libhyperdbg/CMakeLists.txt`'s `if(UNIX)` block
   does `list(REMOVE_ITEM ...)` + `list(APPEND ...)` to swap them.
   Examples: `symbol.cpp` → `symbol-linux.cpp`, `pe-parser.cpp` → `pe-parser-linux.cpp`.

Guard style:
- Windows-only code → `#ifdef _WIN32`
- Linux-only additions → `#ifdef __linux__`
- Cross-platform wrappers → `#if defined(_WIN32) / #elif defined(__linux__)` internally

**Golden rule (see README):** don't scatter `#ifdef` through program logic — route
Windows API calls through the platform interface so both OSes share the call site.

---

## Platform interface layer (new)

User-mode abstractions in `include/platform/user/` (`header/` = interface,
`code/` = implementation):

| File | Abstracts | Linux status |
|------|-----------|--------------|
| `platform-lib-calls.{h,c}` | OS lib calls: events, handles, threads, sprintf/vsnprintf, perf counters, get-last-error, process/thread ids & names, OS version, `strnlen`, `DebugBreak`, zero-memory | Mostly implemented; a few stubbed (see TODO) |
| `platform-intrinsics.{h,c}` | CPU ops: `rdtsc`/`rdtscp`, interlocked 64-bit ops, bit-test-and-set | Implemented (GCC builtins) |
| `platform-serial.{h,c}` | Serial byte transport for remote kernel debugging | **Stub** — Linux branch returns false; termios impl TODO |
| `platform-ioctl.{h,c}` | Local kernel-driver IOCTL interface (`PlatformDeviceIoControl`) + device open (`PlatformOpenDevice`) | **Stub** — no Linux kernel module yet; `PlatformOpenDevice` returns `INVALID_HANDLE_VALUE` |
| `platform-signal.{h,c}` | Console control handler (Ctrl-C / Ctrl-Break) | Implemented (blocks signals + `sigwait` thread) |

Kernel-mode equivalents live in `include/platform/kernel/`. Two were extended for
the port because the shared `script-eval/` code compiles in both user and kernel
builds: `PlatformIntrinsics.c` (interlocked 64-bit ops) and `PlatformMem.c`
(`PlatformSprintf`).

Shared, OS-neutral headers:
- `include/platform/general/header/nt-list.h` (new) — NT doubly-linked-list helpers
  (`InitializeListHead`, `InsertHeadList`, `CONTAINING_RECORD`, …) as `static inline`
  for Linux; inert on Windows.
- `include/platform/general/header/Environment.h` — SAL annotations, string typedefs,
  `CTRL_*_EVENT`, `Sleep`, `INFINITE`/`WAIT_OBJECT_0`, `NTAPI`/`WINAPI`, `SOCKET`, etc.
- `include/SDK/headers/BasicTypes.h` — Linux compat typedefs: `WCHAR` (as `UINT16`),
  `LARGE_INTEGER`, `PSIZE_T`, `LONGLONG`, and pointer aliases (`PLONG`, `PULONG`,
  `PDWORD`, `PUCHAR`, …).

---

## Linux-only replacement files (stubs)

| File | Replaces | What it stubs | TODO to make real |
|------|----------|---------------|-------------------|
| `.../script-engine/symbol-linux.cpp` | `symbol.cpp` (DbgHelp + PDB) | All `Symbol*` functions. Only `SymbolConvertNameOrExprToAddress` does real work: parses a plain hex/decimal literal so numeric addresses work. | Real ELF/DWARF symbol parser (libdw / libelf / libbfd). |
| `.../user-level/pe-parser-linux.cpp` | `pe-parser.cpp` (Windows PE format) | The 3 public fns: `PeShowSectionInformationAndDump`, `PeIsPE32BitOr64Bit` (→ FALSE), `PeGetSyscallNumber` (→ 0). | Recreate the Windows `IMAGE_*` headers for Linux, then port `pe-parser.cpp`. Only needed for Windows-target debugging on Linux. |
| `.../driver-loader/install-linux.cpp` | `install.cpp` (SCM driver loader) | The 2 Linux-visible public fns: `ManageDriver` (→ FALSE) and `SetupPathForFileName` (→ FALSE). The 4 `SC_HANDLE` helpers (`InstallDriver`/`RemoveDriver`/`StartDriver`/`StopDriver`) are guarded out of `install.h` on Linux (never referenced there). | `ManageDriver`: load/unload a future HyperDbg Linux kernel module via `finit_module`/`delete_module` (needs CAP_SYS_MODULE). `SetupPathForFileName`: `readlink("/proc/self/exe")` + strip + append + `access()` (generic "find a file beside my binary"; also used by hwdbg). |

All three self-guard with `#ifdef __linux__` and print
`"... is not supported on Linux yet"` at runtime.

---

## Changes so far (files that build on Linux)

Swept from the port markers (`Platform*` calls, `_WIN32` / `__linux__` guards).
"Wrapper sweep" = mechanical rename of a raw Win32 call to its `Platform*`
equivalent, behavior-preserving.

### Build / precompiled header
- `libhyperdbg/pch.h` — the big one: `#ifdef _WIN32` guards around Windows-only
  headers (`dbghelp.h`, SCM, etc.); unconditional includes of the new
  platform headers + `nt-list.h`; include-order fixes. `install.h` is now included
  unconditionally (it was Windows-only) since its Linux-unsafe `SC_HANDLE` decls are
  self-guarded — see below.
- `header/debugger/driver-loader/install.h` — the 4 `SC_HANDLE` driver helpers
  (`InstallDriver`/`RemoveDriver`/`StartDriver`/`StopDriver`) guarded `#ifdef _WIN32`
  (they use the Windows-only `SC_HANDLE` type and have no callers outside install.cpp);
  `ManageDriver`, `SetupPathForFileName` and the `DRIVER_FUNC_*` macros stay visible on
  both so the Linux callers (`libhyperdbg.cpp`, hwdbg, export.cpp) compile.
- `CMakeLists.txt` (top-level) — `if(LINUX)` branch builds only `script-engine`,
  `libhyperdbg`, `hyperdbg-cli`; links `Threads::Threads`. (CMake is **Linux-only**;
  Windows builds from the `.vcxproj` / MSBuild.)
- `libhyperdbg/CMakeLists.txt` — `if(UNIX)` swaps `symbol.cpp`→`symbol-linux.cpp`,
  `pe-parser.cpp`→`pe-parser-linux.cpp`, and `install.cpp`→`install-linux.cpp`; header
  entries point at the real nested paths.

### script-engine subproject
- GCC-compatibility fixes across `script-engine/` (`pch.h`, `type.h`, `scanner.h`,
  `globals.{h,c}`, `script-engine.c`, its `CMakeLists.txt`) so the shared
  script-engine builds as a Linux `.so`.
- SDK import/interface headers (`include/SDK/HyperDbgSdk.h`,
  `include/SDK/imports/user/HyperDbg*Imports.h`) adjusted for the Linux build.

### Kernel-level debugger (remote protocol)
- `kd.cpp` — largest sweep (~46 `Platform*`): serial open/configure/close via
  `PlatformSerial*`; events/threads/handles via `Platform*`; `RtlZeroMemory`,
  `DeviceIoControl`, `GetLastError`, `GetCurrentProcessId` sweeps; the raw Win32
  serial data-path functions kept under `#ifdef _WIN32` with a Linux interface `#else`.
- `kernel-listening.cpp` — `RtlZeroMemory`, `strnlen_s`→`PlatformStrnlen`, serial
  wait/read via `PlatformSerial*`, `DebugBreak`→`PlatformDebugBreak`.
- `readmem.cpp` — `ZeroMemory` / `DeviceIoControl` / `GetLastError` sweep.

### Core debugger
- `debugger.cpp` — `DeviceIoControl` / `GetLastError` / `RtlZeroMemory` sweep;
  `DebuggerGetNtoskrnlBase` body guarded `#ifdef _WIN32` (Linux returns NULL).
- `interpreter.cpp` — `SetConsoleCtrlHandler`→`PlatformInstallCtrlHandler`;
  script-engine message-callback cast.
- `break-control.cpp` — console-control handler routed through `platform-signal`.

### App / export layer
- `export.cpp` — `strcpy_s`×2 → `PlatformStrCpy` (new bounded-copy wrapper; also
  unblocked once `SetupPathForFileName` became visible via the install-linux swap).
- `platform-lib-calls.{h,c}` — added `PlatformStrCpy(Dest, DestSize, Src)`: Windows
  `strcpy_s`; Linux does the same bounds check (empty-string + non-zero on overflow)
  since glibc has no `strcpy_s`. ⚠️ Linux branch **not yet tested** against the exact
  `strcpy_s` semantics — verify before relying on it.
- `platform-lib-calls.{h,c}` — added `PlatformCopyMemory(Destination, Source, Size)`:
  Windows `RtlCopyMemory`; Linux `memcpy` (same arg order/signature).

### hwdbg
- `hwdbg-interpreter.cpp` — `RtlCopyMemory`→`PlatformCopyMemory`, `RtlZeroMemory`→`PlatformZeroMemory`.

### objects
- `objects.cpp` — wrapper sweep: `RtlCopyMemory`×2→`PlatformCopyMemory`,
  `RtlZeroMemory`→`PlatformZeroMemory`, `DeviceIoControl`×4→`PlatformDeviceIoControl`,
  `GetLastError`×4 (the `"ioctl failed"` sites)→`PlatformGetLastError`; plus the two
  enum-first-member `= {0}`→`= {}` value-init fixes (lines 30/31; line 145's struct
  isn't enum-first, left as `= {0}`).

### rev
- `rev-ctrl.cpp` — `DeviceIoControl`→`PlatformDeviceIoControl`, `GetLastError`→`PlatformGetLastError`.

### App
- `dllmain.cpp` — whole `DllMain` body guarded `#ifdef _WIN32` (Windows DLL loader
  entry point; no Linux equivalent, no callers in our code, body was a no-op). Linux
  TU is intentionally empty.
- `libhyperdbg.cpp` — the main app (load/unload driver, open device, event loop).
  Wrapper sweeps: `DeviceIoControl`×5→`PlatformDeviceIoControl`, `GetLastError`×7→`PlatformGetLastError`,
  `CloseHandle`×4→`PlatformCloseHandle`, `WaitForSingleObject`→`PlatformWaitForSingleObject`,
  `CreateEvent(NULL,FALSE,FALSE,NULL)`→`PlatformCreateEvent(FALSE,FALSE)`,
  `CreateThread(...)`→`PlatformCreateThread(fn,NULL)`, and the 2-arg `strcpy_s(g_DriverName, ...)`
  template form→`PlatformStrCpy(g_DriverName, sizeof(g_DriverName), ...)`. The local-driver
  device open (`CreateFileA("\\.\HyperDbgDebuggerDevice", ...)`) → new `PlatformOpenDevice`
  wrapper (see platform-ioctl); the surrounding error-handling block stays at the call site
  (`ERROR_ACCESS_DENIED`/`ERROR_GEN_FAILURE` added to `Environment.h` so it compiles on Linux).
  `WindowsSetDebugPrivilege` now resolves via `windows-privilege.c` (see below).
- `windows-only/windows-privilege.{c,h}` — `WindowsSetDebugPrivilege` was already ported
  (Windows: token/SeDebugPrivilege; Linux branch: `return TRUE`), just not wired into the
  Linux build. Added `windows-privilege.c` to `libhyperdbg/CMakeLists.txt` (both the source
  list and the `LANGUAGE CXX` block); un-guarded its header include in `pch.h` (was
  `#ifdef _WIN32`, header is Linux-safe); fixed the header's `#ifdef __linux__` SDK include
  path (`../../../../` → `../../../../../`, it sits one dir deeper in `windows-only/`).

### User-level debugger
- `ud.cpp` — wrapper sweep (bucket 1): `DeviceIoControl`→`PlatformDeviceIoControl`,
  the `"ioctl failed"` `GetLastError`→`PlatformGetLastError`, `RtlZeroMemory`→`PlatformZeroMemory`,
  the event-handle `CloseHandle`→`PlatformCloseHandle`, `CreateEvent(NULL,x,y,NULL)`→`PlatformCreateEvent(x,y)`.
  Win32 process/thread-management (bucket 2): 5 new `Platform*` process wrappers (Group A)
  + whole-body `#ifdef _WIN32` guards on the Toolhelp walkers / `UdPrintError` (Group B).
  See the Process-control section of the TODO ledger for details.
- `platform-lib-calls.{h,c}` — added `PlatformCreateProcess`/`PlatformOpenProcess`/
  `PlatformTerminateProcess`/`PlatformResumeThread`/`PlatformGetExitCodeProcess`
  (Windows real, Linux stub).

### Script engine
- `script-engine-wrapper.cpp` — 6× `RtlZeroMemory`→`PlatformZeroMemory`; the two
  wide-char test harnesses (`AllocateStructForCasting`, `ScriptEngineWrapperTestParser`)
  guarded out on Linux (see wide-char TODO).
- `script-eval/Functions.c` — `sprintf_s`, `__rdtsc(p)`, `Interlocked*`,
  `RtlZeroMemory`, `QueryPerformance*` → `Platform*`/`Cpu*` wrappers.
- `script-eval/PseudoRegisters.c` — `$peb` guarded `#ifdef _WIN32` (returns 0);
  `$tid`/`$pid`/`$core`/`$pname` → new `Platform*` wrappers.

### Commands & app (wrapper sweeps + wide-char casts)
- Meta: `dump.cpp`, `pagein.cpp`, `pe.cpp`, `start.cpp`, `restart.cpp`
  (the last two carry `(WCHAR *)` wide-char shim casts).
- Debugging/extension: `a.cpp`, `dt-struct.cpp`, `k.cpp`, `preactivate.cpp`,
  `prealloc.cpp`, `sleep.cpp`, `track.cpp`, `pci-id.cpp`, `pcicam.cpp`, `pcitree.cpp`.
- App: `messaging.cpp`, `packets.cpp` (`vsprintf_s`→`PlatformVsnprintf`),
  `spinlock.cpp` (`_interlockedbittestandset`→`CpuInterlockedBitTestAndSet`).
  (`libhyperdbg.cpp` itself has its own entry under **App** above.)

---

## TODO ledger — revisit before Linux is functional

Grouped by subsystem. These are the shortcuts taken to reach compilation.

### Wide characters (the big deferred item)
- [ ] **`wchar_t` 2-vs-4-byte / `WCHAR` / `UNICODE_STRING`.** On Linux `WCHAR` is
  2 bytes but native `wchar_t` is 4. Current state: bogus 2-byte reinterpret casts
  at call sites (each marked `TEMPORARY LINUX SHIM / TODO(Linux)`), and the
  `script-engine-wrapper.cpp` test harnesses are guarded out on Linux. Needs a
  real `std::wstring` → 2-byte-`WCHAR`/UTF-16 conversion helper before Linux file
  I/O and the user-debugger path can actually open files.

### Symbols
- [ ] Replace `symbol-linux.cpp` stubs with a real ELF/DWARF symbol parser.

### PE parsing
- [ ] Recreate Windows `IMAGE_*` headers for Linux and port `pe-parser.cpp`
  (replace `pe-parser-linux.cpp`). Affects `!pe`; `!hide` currently gets `0` for
  all syscall numbers (a Windows-guest feature, meaningless on a Linux host).

### Transport
- [ ] `platform-serial.c` Linux branch — implement termios serial I/O (currently stub).
- [ ] `platform-ioctl.c` Linux branch — needs a Linux kernel module + real ioctl
  (currently stub). This is the local driver interface used across many files.

### Process control — `ud.cpp` DONE (2026-07-18)
Mechanical wrapper sweep (bucket 1): `DeviceIoControl`×10→`PlatformDeviceIoControl`,
`GetLastError`×10 (the `"ioctl failed"` sites)→`PlatformGetLastError`,
`RtlZeroMemory`×3→`PlatformZeroMemory`, `CloseHandle`×1 (event-handle close)→`PlatformCloseHandle`,
`CreateEvent(NULL,FALSE,FALSE,NULL)`→`PlatformCreateEvent(FALSE,FALSE)`.

Bucket 2 (Win32 process/thread mgmt) resolved two ways (user decision):
- **Group A — new guarded `Platform*` wrappers** (real on Windows, Linux stub) for the
  self-contained calls: `PlatformCreateProcess` (keeps `STARTUPINFO` internal),
  `PlatformOpenProcess`, `PlatformTerminateProcess`, `PlatformResumeThread`,
  `PlatformGetExitCodeProcess` — all in `platform-lib-calls.{h,c}`. ud.cpp call sites
  swapped 1:1; `UdCreateSuspendedProcess` now calls `PlatformCreateProcess` with the
  `CREATE_SUSPENDED|CREATE_NEW_CONSOLE` flags kept at the call site.
- **Group B — whole-body `#ifdef _WIN32` guards** (Windows verbatim, Linux stub) for the
  calls interleaved with UI/walk logic: `UdListProcessThreads`, `UdCheckThreadByProcessId`
  (Toolhelp snapshot walk), `UdPrintError` (`FormatMessage`/`MAKELANGID`). No Linux
  Toolhelp/`THREADENTRY32` types needed.
- **Pure additions:** Linux `PROCESS_INFORMATION` struct in `SDK/headers/BasicTypes.h`;
  `PROCESS_TERMINATE`/`PROCESS_QUERY_LIMITED_INFORMATION`/`CREATE_SUSPENDED`/
  `CREATE_NEW_CONSOLE`/`STILL_ACTIVE` `#define`s in `Environment.h` (Linux block).

TODO(Linux) still open in the wrapper bodies: real `fork`+`execve`/`ptrace` process
backend, and the Toolhelp thread-enumeration equivalent (`/proc`) — all stubbed for now.

### Misc runtime stubs
- [ ] `PlatformGetOsVersion` — Linux returns FALSE; implement via `uname`.
- [ ] `$peb` pseudo-register — returns 0 on Linux (PEB is NT-only).
- [ ] `DebuggerGetNtoskrnlBase` — returns NULL on Linux (NT system-module enum).
- [ ] File I/O / user-debugger paths — stubbed (also blocked on wide-char above).

### Build system
- [ ] Add `.gitignore` rules for the in-source CMake build output
  (`CMakeCache.txt`, `CMakeFiles/`, generated `Makefile`, `cmake_install.cmake`,
  `*.o`, `*.so`) — or switch to an out-of-source `build/` directory.

---

## Building

```bash
cmake .   # re-run only when CMake files change
make      # build; find the next file that fails, port it, repeat
```
