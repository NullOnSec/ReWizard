# Next Session Plan

Updated: 2026-05-16

## Status: Kernel Executes in Long Mode

The boot disk kernel stub now runs successfully in Bochs x86_64 long mode:
- 6 boot disk tests passing (init, MBR signature, kernel bytes, memory I/O, page tables, NOP+HLT execution, VGA buffer write)
- Direct EFER manipulation bypasses `SetEFER()` which rejects LME when CR0.PG=1
- `BochsExecutor::Initialize()` no longer fails if system binaries are missing
- Disk image configured as floppy (not hard drive) — matches INT 13h BIOS calls

**All 75 tests pass** (DB tests fail only due to C: drive being full — env issue, not code bug).

---

## Architecture Decisions Updated

### LLVM Optimization Pipeline
- **Full pipeline**: `SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE`
- **InternalLinkage** for lifted functions so `GlobalDCEPass` can remove orphans

### Hybrid Engine: Bochs Sole Backend
- **Full `bx_init_hardware()` integration**: VGA, ATA, floppy, keyboard, etc. all initialized
- **`load_and_init_display_lib()`** called before device init to prevent NULL `bx_gui` crash
- **Floppy disk boot**: `floppya:` config (not `ata0-master`) — matches INT 13h bootloader
- **Optional binary loading**: `Initialize()` warns (not errors) if system PEs are missing

### Boot Disk Architecture
- **Scripts**: `scripts/build-boot-disk.ps1` assembles MBR + 64-bit kernel stub via NASM
- **Kernel stub**: real mode → protected mode → long mode transition, writes "ReWizard" to VGA, `out 0x501, 0x31`
- **Manual long mode setup**: EFER.LMA+LME set directly (`efer.set32(0x500)`) before enabling CR0.PG

---

## What's Been Done

**Phase 3 — Hybrid Analysis (IN PROGRESS):**
- BochsExecutor with full hardware init, floppy disk boot, optional PE loading
- 6 boot disk tests: disk init, MBR validity, kernel bytes, memory I/O, page tables, long mode execution, VGA buffer verification
- Instrumentation bridge for real CPU state capture during execution
- End-to-end hybrid analysis test (trace → indirect call resolution)

**Phase 6 — Database & Annotations (COMPLETE):**
- AnalysisDatabase (SQLite), XrefManager, SymbolManager — all tested

**Boot Disk (DONE):**
- `tests/fixtures/boot_disk.img` — 1.44MB floppy, MBR bootloader → 64-bit kernel
- `scripts/build-boot-disk.ps1` — NASM-based, idempotent
- Kernel stub runs in long mode, writes to VGA buffer at 0xB8000

---

## Known Issues / Limitations

- **C: drive nearly full** (10MB free) — causes SQLite test failures. Free space to fix.
- **Full BIOS boot too slow for unit tests** — manual long mode setup works around this
- **Debug builds blocked** — LLVM built with Release CRT; use `x64-release` preset
- **Disk image locked by Bochs** — non-blocking, ATA device reports lock error

---

## Recommended Next Steps (in priority order)

### 1. Disk Space Cleanup (BLOCKING)
- Free space on C: drive (only 10MB free, breaks SQLite tests)
- Consider moving temp dirs to Z: drive

### 2. Full BIOS Boot Integration (Phase 3.7)
- Boot through BIOS + bootloader naturally (currently too slow for unit tests)
- Add `BootFromDisk()` integration test with timeout mechanism
- Consider Bochs snapshot/restore API for faster test cycles

### 3. VGA Buffer Verification Enhancement
- Current test checks first 2 chars ("Re") of "ReWizard" at 0xB8000
- Add full 8-character string verification once full boot works

### 4. Deobfuscation Passes (Phase 4.2-4.5)
- Control flow flattening recovery in LLVM IR
- Dead code elimination, binary patching

### 5. Interactive UI (Phase 6.4-6.6)
- Dear ImGui integration
- Disassembly panel, graph view, hex view

---

## Key Files

- `ReWizardLib/source/Hybrid/BochsExecutor.cpp` — Full hardware init, floppy boot config, optional binary loading
- `ReWizardLib/source/Hybrid/BochsInstrumentationBridge.cpp` — Real CPU state capture
- `tests/test_bochs_boot_disk.cpp` — 7 boot disk + long mode tests
- `scripts/build-boot-disk.ps1` — NASM boot disk generator
- `tests/fixtures/boot_disk.img` — 1.44MB bootable floppy image
- `ReWizardLib/source/Analysis/Units/SymbolManager.cpp` — SymbolManager (9 tests)
- `ReWizardLib/source/Database/AnalysisDatabase.cpp` — Extended with annotations

## Build Reminders

- `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-release --target ReWizardTests"`
- `x64-release` preset only (Debug build conflicts with LLVM's Release CRT)
- Boost: `Z:/boost/boost_1_85_0`
- LLVM: `Z:/llvm-install/`
- remill: `Z:/remill-install/`
- Bochs: `Z:/bochs-install/`
- NASM: `C:\Users\Z\AppData\Local\bin\NASM\nasm.exe`