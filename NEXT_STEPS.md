# Next Session Plan

Updated: 2026-05-16

## P0 BUG: ConstantFoldingPass Crash (0xc0000005 / Access Violation)

### Status: FIXED

**Full pipeline: `SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE`**

All 74 tests pass (including 5 new boot disk tests).

---

## Architecture Decisions Updated

### LLVM Optimization Pipeline
- **Full pipeline enabled**: `SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE`
- **InternalLinkage**: All lifted functions use `InternalLinkage` so `GlobalDCEPass` can remove orphans
- **verifyModule()**: IR validation before and after optimization catches malformed IR early

### Hybrid Engine: Bochs Sole Backend
- **PANDAS is OUT** — Linux-only host, requires QEMU build, cannot run on Windows
- **Unicorn is OUT** — removed entirely; remill + static analysis covers the use cases
- **Bochs is IN as sole backend** — full-system emulation, no API stubs needed
- **No Python scripts** — everything embedded in C++

### IR: LLVM IR with remill (manual lifter deprecated)
- **Single IR, single toolchain** — LLVM IR for everything
- **Manual Zydis+IRBuilder lifter DEPRECATED** — only handled 6/~1500 mnemonics, fundamentally incomplete
- **remill (Trail of Bits, v6.0.1, Apache-2.0)** chosen as the sole binary lifter — comprehensive x86/x86_64 semantics

---

## What's Been Done

**Phase 0 — Stabilization (COMPLETE):**
- Fixed Win32InternalTypes.hpp, AnalysisManager UB, PassManager threading, CMake WHOLEARCHIVE, CLI arguments, tests

**Phase 1 — Import/Export Analysis (COMPLETE):**
- SymbolTable, ImportAnalysisPass (PE), symbol annotations, PE relocation fixups, tests

**Phase 2 — Improved Static Analysis (COMPLETE):**
- Recursive descent disassembly, data flow analysis, abstract interpretation, opaque predicate detection, output generation

**Phase 3 — Hybrid Analysis (IN PROGRESS):**
- Trace infrastructure, platform abstraction, HybridAnalysisPass, SimpleTraceReader O(1) lookup
- Pass dependency system — topological sort fixes critical bug where 4/6 passes silently no-opped
- Bochs integration: `BochsExecutor` and `BochsInstrumentationBridge` implemented
- Full Bochs 3.0 build with instrumentation enabled, libraries at `Z:/bochs-install`
- **BochsExecutor::Initialize() rewritten to use `bx_init_hardware()`** — full device initialization with VGA, ATA, etc.
- **`load_and_init_display_lib()` called before device init** — prevents NULL `bx_gui` crash
- **`SetDiskImage()` / `BootFromDisk()`** for Windows disk boot support
- **Binary loading made optional** — `Initialize()` no longer fails if system binaries are missing
- **Minimal boot disk test suite**: 5 tests covering disk image loading, MBR signature, kernel stub bytes, memory write/read, and page table setup
- **End-to-end hybrid analysis test completed**: `test_bochs_hybrid_e2e.cpp` — trace capture, indirect call resolution

**Phase 4 — IR & Deobfuscation (COMPLETE):**
- LLVM 19.1.0 built from source, C++ libraries linked
- **remill v6.0.1 integrated** as the sole binary lifter (replaced manual lifter)
- IRLiftingPass auto-registered, lifts all basic blocks into single shared module
- ConstantFoldingPass runs full optimization pipeline

**Phase 6 — UI/Database (IN PROGRESS):**
- **DONE 6.1**: `AnalysisDatabase` with SQLite backend (8 tests)
- **DONE 6.2**: `XrefManager` — bidirectional in-memory xref index (8 tests)
- **DONE 6.3**: `SymbolManager` — user-defined names, comments, types with SQLite persistence (9 tests)
- **DONE**: Boot disk image generation (`scripts/build-boot-disk.ps1`) — MBR bootloader → long mode → kernel stub

---

## Known Issues / Limitations

- **Debug builds blocked**: LLVM built with `/MD` (Release CRT), project Debug uses `/MDd`. Use `x64-release` preset only.
- **Full BIOS boot too slow for unit tests**: Bochs BIOS POST takes millions of instructions; manual long mode setup in tests works around this
- **Long mode CPU state programming**: Setting up long mode programmatically via Bochs APIs requires careful CR0/CR4/EFER ordering; `SetEFER()` rejects LME when CR0.PG is set
- **Disk image locked by Bochs**: When Bochs opens the disk image for ATA emulation, it locks the file; subsequent tests can still initialize but the disk device reports a locked image (non-blocking)
- **Thread safety** in `PassManager::RunAllAsync` — detached thread + `std::promise` lifetime risk

---

## Recommended Next Steps (in priority order)

### 1. Full BIOS Boot Integration (Phase 3.7)

- Boot disk image proven; next step is real Windows boot
- Need larger disk image with Windows PE or minimal Windows
- `BootFromDisk()` method exists but full BIOS POST is too slow for automated tests
- Consider timeout mechanism or snapshot-based approach for faster test cycles

### 2. Symbol & Annotation Persistence (Phase 6.3)

- `SymbolManager` — user-defined names, typed variables, function signatures, comments
- All annotations stored in database, survive re-analysis passes

### 3. Deobfuscation Passes (Phase 4.2-4.5)

- **Control Flow Flattening Recovery**: Pattern-match dispatcher state variables in LLVM IR
- **Dead Code Elimination**: LLVM AggressiveDCE on remill-generated IR
- **Binary Patching & Code Emission**: LLVM MC layer to emit x86 bytes from simplified IR

### 4. Interactive UI (Phase 6.4-6.6)

- Dear ImGui integration
- Disassembly panel with symbol resolution and xref counts
- Graph view: Boost.Graph-derived CFG visualization

---

## Key Files

- `ReWizardLib/source/Hybrid/BochsExecutor.cpp` — Full hardware init, disk boot support, optional binary loading
- `ReWizardLib/source/Hybrid/BochsInstrumentationBridge.cpp` — Real CPU state capture
- `ReWizardLib/source/Analysis/Units/SymbolManager.cpp` — SymbolManager implementation
- `ReWizardLib/source/Database/AnalysisDatabase.cpp` — Extended with annotation support
- `tests/test_bochs_boot_disk.cpp` — 5 boot disk tests (init, MBR signature, kernel bytes, memory I/O, page tables)
- `tests/test_bochs_hybrid_e2e.cpp` — End-to-end Bochs trace → hybrid analysis
- `scripts/build-boot-disk.ps1` — Generates bootable floppy image with NASM

## Build Reminders

- Use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && <command>"` (x64, not x86)
- Release preset required when LLVM enabled (Debug CRT mismatch)
- Boost at `Z:/boost/boost_1_85_0`, override with `-DBOOST_ROOT=Z:/boost/boost_1_85_0`
- LLVM at `Z:/llvm-install/` (full C++ build)
- remill at `Z:/remill-install/`
- Bochs at `Z:/bochs-install/`
- NASM at `C:\Users\Z\AppData\Local\bin\NASM\nasm.exe`