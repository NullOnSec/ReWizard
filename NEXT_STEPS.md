# Next Session Plan

Updated: 2026-05-16

## P0 BUG: ConstantFoldingPass Crash (0xc0000005 / Access Violation)

### Status: FIXED

**Full pipeline: `SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE`**

All 56 tests pass (3 skipped).

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
- **remill (Trail of Bits, v6.0.1, Apache-2.0)** chosen as the binary lifter — comprehensive x86/x86_64 semantics

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
- **Bochs init test** verifies siminterface, options, config parsing, and plugin loading
- **Known blocker**: `bx_init_hardware()` crashes with 0xc0000005 during device init (likely `BX_INFO` or `bx_gui` due to static plugin linking issues)

**Phase 4 — IR & Deobfuscation (COMPLETE):**
- LLVM 19.1.0 built from source, C++ libraries linked
- **remill v6.0.1 integrated** as the sole binary lifter (replaced manual lifter)
- IRLiftingPass auto-registered, lifts all basic blocks into single shared module
- ConstantFoldingPass runs full optimization pipeline: SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE
- All CFG recovery bugs fixed (mega-function, IAT entry points, off-by-one, tail-call detection)
- Removed Unicorn dependency entirely from project (CMake, source, docs)

**Phase 6 — UI/Database (IN PROGRESS):**
- `ReWizardUI/` target created with stub `main.cpp`
- UI designated as **primary entry point**, CLI as secondary
- **DONE 6.1**: `AnalysisDatabase` with SQLite backend
  - Schema: functions, basic_blocks, instructions, symbols, xrefs
  - CRUD operations, transactions, upsert support
  - FetchContent integration for SQLite3 amalgamation
  - 8 database tests (all passing)
- **DONE 6.2**: `XrefManager` — bidirectional in-memory xref index
  - O(1) lookup by from/to address
  - Typed queries: calls, jumps, data refs
  - Persistence to/from `AnalysisDatabase`
  - `StaticControlFlowRebuilder` populates xrefs during CFG recovery
  - 8 xref tests (all passing)

---

## Known Issues / Limitations

- **JMP tail calls to undiscovered functions**: If a JMP target hasn't been discovered as a function yet, it will be inlined into the current function. Only JMPs to already-known function starts are correctly treated as tail calls.
- **Debug builds blocked**: LLVM built with `/MD` (Release CRT), project Debug uses `/MDd`. Use `x64-release` preset only.
- **`sub_1400021b0`**: Found by `LinearSweepFallback` in merged section — not a real function but passes exec check.
- **Bochs hardware init crash**: `bx_init_hardware()` crashes during device init. Likely caused by `genlog`/`theVga` null dereference or missing static plugin constructors for `nogui` display library.
- **Thread safety** in `PassManager::RunAllAsync` — detached thread + `std::promise` lifetime risk.
- **WHOLEARCHIVE CMake** — the per-pass logic uses incomplete object paths.

---

## Recommended Next Steps (in priority order)

### 1. Bochs Hardware Init Fix (Phase 3.6)

- **Root cause**: `bx_init_hardware()` crashes with 0xc0000005 during `DEV_init_devices()` / VGA init.
- **Hypothesis**: Static plugin linking (`gui.lib`, `iodev_display.lib`) may not pull `nogui.obj`/`vga.obj` constructors because no symbol is explicitly referenced. MSVC `/WHOLEARCHIVE` or explicit symbol reference may be needed.
- **Alternative**: Bypass full `bx_init_hardware()` and manually initialize `BX_CPU_C`, `BX_MEM_C`, and `bx_pc_system` without device emulation.
- Once init is stable:
  - Rewrite `BochsExecutor.cpp` to use real Bochs init, memory mapping, register get/set, icount-guarded execution
  - Update `BochsInstrumentationBridge.cpp` to capture actual CPU state (RIP, GPRs) during `bx_instr_before_execution`/`after_execution`
  - Write end-to-end hybrid analysis test: load PE → static CFG → Bochs trace → verify indirect call resolution

### 2. Symbol & Annotation Persistence (Phase 6.3)

- `SymbolManager` — user-defined names, typed variables, function signatures, comments
- Rename symbols: override auto-generated names (sub_401000 → main)
- Function type annotations: calling convention, return type, parameter types
- Inline comments: attach user notes to any address
- Type system: structs, enums, typedefs
- All annotations stored in database, survive re-analysis passes

### 3. Deobfuscation Passes (Phase 4.2-4.5)

- **Control Flow Flattening Recovery**: Pattern-match dispatcher state variables in LLVM IR
- **Dead Code Elimination**: LLVM AggressiveDCE on remill-generated IR
- **Binary Patching & Code Emission**: LLVM MC layer to emit x86 bytes from simplified IR

### 4. Interactive UI (Phase 6.4-6.6)

- Dear ImGui integration
- Disassembly panel with symbol resolution and xref counts
- Graph view: Boost.Graph-derived CFG visualization
- Hex view: raw bytes with decoded instruction overlay
- Function list & global search

---

## Key Files

- `ReWizardLib/source/Database/AnalysisDatabase.cpp` — SQLite schema, CRUD, transactions
- `ReWizardLib/source/Analysis/Units/XrefManager.cpp` — Bidirectional xref index
- `ReWizardLib/source/Analysis/Passes/StaticControlFlowRebuilder.cpp` — CFG recovery, xref population
- `ReWizardLib/source/IR/Lifter.cpp` — remill-based lifter
- `ReWizardLib/source/IR/LLVMOptimizer.cpp` — Full pipeline: SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE
- `ReWizardLib/source/Analysis/Passes/StaticControlFlowRebuilder.cpp` — CALL fix, IAT removal, exec-section guards, JMP tail-call detection
- `ReWizardLib/source/Hybrid/BochsExecutor.cpp` — Bochs integration (needs rewrite for real init)
- `ReWizardLib/source/Hybrid/BochsInstrumentationBridge.cpp` — Custom instrumentation (needs real CPU state capture)
- `tests/test_database.cpp` — Database schema, function CRUD, transactions, persistence
- `tests/test_xref_manager.cpp` — Xref add/remove/query, save/load, typed filtering
- `tests/test_bochs_init.cpp` — Bochs siminterface, options, config parsing test
- `scripts/build-llvm.ps1` — Idempotent LLVM 19.1.0 build script
- `scripts/build-remill.ps1` — Idempotent remill v6.0.1 build script with x86-only patches

## Build Reminders

- Use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && <command>"` (x64, not x86)
- Release preset required when LLVM enabled (Debug CRT mismatch)
- Boost at `Z:/boost/boost_1_85_0`, override with `-DBOOST_ROOT=Z:/boost/boost_1_85_0`
- LLVM at `Z:/llvm-install/` (full C++ build), fallback `Z:/llvm/` (C API only)
- remill at `Z:/remill-install/` (built via `scripts/build-remill.ps1`)
- Bochs at `Z:/bochs-install/` (built via `scripts/build-bochs.ps1`)
