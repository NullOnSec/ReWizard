# Next Session Plan

Updated: 2026-05-15

## Critical Bug Discovered

**Pass execution order is broken.** Passes run in `std::map` order (alphabetical by name), causing 4 out of 6 passes to silently do nothing:

| # | Current Order | Problem |
|---|---|---|
| 1 | `AbstractInterpretationPass` | **No-op** — iterates empty function list (CFG not built yet) |
| 2 | `DataFlowAnalysisPass` | **No-op** — same reason |
| 3 | `HybridAnalysisPass` | **No-op** — no functions are marked |
| 4 | `ImportAnalysisPass` | Works (populates symbol table) |
| 5 | `OpaquePredicatePass` | **No-op** — reads opaque predicate data never set |
| 6 | `StaticControlFlowRebuilder` | Works (builds CFG) — but runs LAST |

**Correct order:** ImportAnalysisPass → StaticControlFlowRebuilder → DataFlowAnalysisPass → AbstractInterpretationPass → OpaquePredicatePass → HybridAnalysisPass

## Architecture Decisions Updated

### Hybrid Engine: Bochs Primary, Unicorn Optional

- **PANDAS is OUT** — Linux-only host, requires QEMU build, cannot run on Windows
- **Bochs is IN as primary backend** — full-system emulation, no API stubs needed, works on Windows/Linux/macOS
- **Unicorn is IN as optional accelerator** — for simple micro-execution (arithmetic predicates), with syscall service layer for common NT calls. Never the foundation.
- **No Python scripts** — everything embedded in C++. No "record on Linux / analyze on Windows" workflow.
- **Bochs snapshot strategy** — boot once, save CPU+memory state, restore for each analysis session

### Bochs Rationale

The binspektor prototype needed **20 hooks (13 unique implementations) just for hello-world**. Real binaries hit hundreds of APIs. The syscall service layer (~25-30 NT handlers) reduces this but is still a maintenance trap — complex syscalls like `NtAllocateVirtualMemory` have nuanced semantics, and CRT initialization, TLS callbacks, and SEH all need stubs. Bochs provides **correctness by default** with full OS emulation. Snapshot mitigates the boot-time penalty.

## What's Been Done

**Phase 0 — Stabilization (COMPLETE):**
- Fixed Win32InternalTypes.hpp, AnalysisManager UB, PassManager threading, CMake WHOLEARCHIVE, CLI arguments, tests

**Phase 1 — Import/Export Analysis (COMPLETE):**
- SymbolTable, ImportAnalysisPass (PE), symbol annotations in disassembly, PE relocation fixups, tests

**Phase 2 — Improved Static Analysis (COMPLETE):**
- ✅ 2.1 Recursive Descent Disassembly — entrypoint + exports + IAT targets, call following, linear sweep fallback
- ✅ 2.2 Data Flow Analysis — register tracking (mov/lea/xor), indirect call/jump resolution via known register values
- ✅ 2.3 Abstract Interpretation — interval/domain analysis, opaque predicate detection (test/cmp + jz/jnz/js/jns patterns)
- ✅ 2.4 Output Generation — AnalysisResult with JSON/DOT/text export, CLI --output/--format flags

**Phase 3 — Hybrid Analysis (IN PROGRESS):**
- ✅ 3.1 Trace Infrastructure — `Hybrid/TraceRecord.h`, `ITraceReader.h`, `SimpleTraceReader` (JSON trace format)
- ✅ 3.3 HybridAnalysisPass — consumes trace data, resolves indirect targets, clears disproven opaque predicates, marks hybrid-verified
- ✅ 3.4 Hybrid/Static Iteration — `StaticControlFlowRebuilder::ReAnalyzeFrom()` called from HybridAnalysisPass
- ✅ 3.7 SimpleTraceReader O(1) PC Lookup — unordered_map index, O(1) GetRecordsForPC()
- ⏳ 3.5 Bochs Integration — IEmulator interface, BochsExecutor, snapshot management
- ⏳ 3.6 UnicornExecutor (optional) — micro-execution fast-path with syscall service layer
- ❌ 3.2 PANDAS Trace Recording Workflow — **REJECTED**: Linux-only, cannot run on Windows

**Phase 4 — Deobfuscation (IN PROGRESS):**
- ✅ 4.1 OpaquePredicatePass — removes dead successors from always-true/always-false branches, rebuilds CFG
- ⏳ 4.2 DeobfuscationFlattenPass — pattern-match control-flow flattening dispatchers
- ⏳ 4.3 DeadCodeEliminationPass — remove unreachable basic blocks
- ⏳ 4.4 ConstantFoldingPass — evaluate constant expressions

**Phase 5 — Extended Features:**
- ⏳ Not started

## Recommended Next Steps (in priority order)

### ✅ DONE — Pass Dependency System (P0 — Critical Bug Fix)
- Added `Dependencies()` to `BaseAnalysisPass`, overridden by all 6 passes
- Implemented Kahn's topological sort in `PassManager::RunAll()`
- Passes now execute in correct dependency order
- All 31 tests pass

### ✅ DONE — MemoryMapper Platform Abstraction
- Extracted `MemoryMapper` interface with Map/Unmap/Zero methods
- Implemented `Win32MemoryMapper` (VirtualAlloc/VirtualFree) and `PosixMemoryMapper` (mmap/munmap)
- `FileLoader` takes `MemoryMapper` via constructor; auto-creates platform-specific impl
- PosixMemoryMapper excluded from Windows build via CMake conditional
- All 31 tests pass

### ✅ DONE — SimpleTraceReader O(1) PC Lookup
- Added `unordered_map<uintptr_t, vector<size_t>>` index built during `Load()`
- `GetRecordsForPC()` now O(1) average case instead of O(n) linear scan
- All 31 tests pass

### 1. Bochs Integration Architecture (NEXT MAJOR TASK)
- Design `IEmulator` / `ITraceProducer` interface
- Add Bochs as FetchContent/prebuilt dependency (LGPL v2.1)
- `BochsExecutor` implementation:
  - VM lifecycle management (boot, snapshot, restore)
  - Instrumentation hooks (`bx_instr_before_execution`)
  - Breakpoint-driven execution (execute only target function)
  - Yield `TraceRecord`s compatible with existing `HybridAnalysisPass`
- Snapshot format: CPU state + memory regions + device state

### 2. UnicornExecutor (Optional Accelerator)
- Encapsulate binspektor prototype as `UnicornExecutor` implementing `IEmulator`
- Syscall service layer: table-driven NT syscall handlers (~25-30 common)
- Only for simple micro-execution (arithmetic predicates, short code regions)
- Falls back to Bochs for unhandled cases (future)

### 3. Phase 4 Deobfuscation Passes
- 4.2 DeobfuscationFlattenPass, 4.3 DeadCodeEliminationPass, 4.4 ConstantFoldingPass

## Files Recently Modified

- `ReWizardLib/include/ReWizard/Analysis/Passes/OpaquePredicatePass.h` (new)
- `ReWizardLib/source/Analysis/Passes/OpaquePredicatePass.cpp` (new)
- `ReWizardLib/include/ReWizard/Analysis/Units/BasicBlock.h` (added RemoveSuccessor)
- `ReWizardLib/include/ReWizard/Analysis/Units/Function.h` (added opaquePredicateResults_, hybridVerified_)
- `ReWizardLib/source/Analysis/Passes/StaticControlFlowRebuilder.cpp` (added ReAnalyzeFrom)
- `tests/test_opaque_predicate.cpp` (new)
- `tests/test_hybrid_analysis.cpp` (added ResolvedTargetTriggersReAnalysis)

## Build Reminders

- Always use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && <command>"`
- Boost is at `Z:/boost/boost_1_85_0`
- Commit small, focused changes only
- Test before every commit
