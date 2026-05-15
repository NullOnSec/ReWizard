# Next Session Plan

Updated: 2026-05-15

## Architecture Decisions Updated

### Hybrid Engine: Bochs Primary, Unicorn Optional

- **PANDAS is OUT** — Linux-only host, requires QEMU build, cannot run on Windows
- **Bochs is IN as primary backend** — full-system emulation, no API stubs needed, works on Windows/Linux/macOS
- **Unicorn is IN as optional accelerator** — for simple micro-execution (arithmetic predicates), with syscall service layer for common NT calls. Never the foundation.
- **No Python scripts** — everything embedded in C++. No "record on Linux / analyze on Windows" workflow.
- **Bochs snapshot strategy** — boot once, save CPU+memory state, restore for each analysis session

### IR: VEX IR for Deobfuscation

- **VEX IR is the chosen IR** for deobfuscation (constant folding, dead code elimination, CFF flattening recovery)
- **LLLVM is OUT** — too large/heavyweight for our use case
- **No custom lifters** — must use readily available, maintained code only
- angr's `libvex` (from pyvex C core) compiles with MSVC, BSD-2-Clause license, supports x86/AMD64/ARM/ARM64/MIPS/PPC
- Deobfuscation passes operate on VEX IR statements (WrTmp, Put, Store, Exit) instead of raw x86 instructions
- This makes transformations architecture-independent and sound (all side-effects explicit in IR)
- Multi-architecture future-proof: same deobfuscation passes work on ARM, MIPS, etc.

### Bochs Rationale

The binspektor prototype needed **20 hooks (13 unique implementations) just for hello-world**. Real binaries hit hundreds of APIs. The syscall service layer (~25-30 NT handlers) reduces this but is still a maintenance trap. Bochs provides **correctness by default** with full OS emulation. Snapshot mitigates the boot-time penalty.

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
- ✅ 3.2 Platform Abstraction — `MemoryMapper` interface, `Win32MemoryMapper`, `PosixMemoryMapper`
- ✅ 3.3 HybridAnalysisPass — consumes trace data, resolves indirect targets, clears disproven opaque predicates, marks hybrid-verified
- ✅ 3.4 Hybrid/Static Iteration — `StaticControlFlowRebuilder::ReAnalyzeFrom()` called from HybridAnalysisPass
- ✅ 3.7 SimpleTraceReader O(1) PC Lookup — unordered_map index, O(1) GetRecordsForPC()
- ✅ Pass dependency system — topological sort fixes critical bug where 4/6 passes silently no-opped
- ⏳ 3.5 Bochs Integration — IEmulator interface, BochsExecutor, snapshot management
- ⏳ 3.6 UnicornExecutor (optional) — micro-execution fast-path with syscall service layer
- ❌ 3.2 PANDAS Trace Recording Workflow — **REJECTED**: Linux-only, cannot run on Windows

**Phase 4 — Deobfuscation (IN PROGRESS):**
- ✅ 4.1 OpaquePredicatePass — removes dead successors from always-true/always-false branches, rebuilds CFG
- ⏳ 4.0 VEX IR Integration — add libvex dependency, create VEXLifter, IRBlock wrapper
- ⏳ 4.2 DeobfuscationFlattenPass — CFF recovery on VEX IR
- ⏳ 4.3 DeadCodeEliminationPass — dead code elimination on VEX IR
- ⏳ 4.4 ConstantFoldingPass — constant propagation and folding on VEX IR

**Phase 5 — Extended Features:**
- ⏳ Not started

## Completed Tasks

### ✅ Pass Dependency System (P0 — Critical Bug Fix)
- Added `Dependencies()` to `BaseAnalysisPass`, overridden by all 6 passes
- Implemented Kahn's topological sort in `PassManager::RunAll()`
- Correct order: ImportAnalysisPass → StaticControlFlowRebuilder → DataFlowAnalysisPass+AbstractInterpretationPass → OpaquePredicatePass → HybridAnalysisPass
- All 31 tests pass

### ✅ MemoryMapper Platform Abstraction
- Extracted `MemoryMapper` interface with Map/Unmap/Zero methods
- Implemented `Win32MemoryMapper` (VirtualAlloc/VirtualFree) and `PosixMemoryMapper` (mmap/munmap)
- `FileLoader` takes `MemoryMapper` via constructor; auto-creates platform-specific impl
- PosixMemoryMapper excluded from Windows build via CMake conditional
- All 31 tests pass

### ✅ SimpleTraceReader O(1) PC Lookup
- Added `unordered_map<uintptr_t, vector<size_t>>` index built during `Load()`
- `GetRecordsForPC()` now O(1) average case instead of O(n) linear scan
- All 31 tests pass

## Recommended Next Steps (in priority order)

### 1. VEX IR Integration (Prerequisite for Phase 4 Deobfuscation)
- Add angr's `libvex` (from pyvex C core) as CMake FetchContent dependency
- Create `IR/VEXLifter.h|.cpp` — wraps `libvex` to lift raw bytes at address → `IRSB`
- Create `IR/IRBlock.h` — C++ wrapper around VEX IR types (IRStmt, IRExpr, IRType)
- Integrate with `BasicBlock`: each BB holds optional `IRBlock` for its lifted IR
- Test: lift known x86 byte sequences (nop, mov, add, jmp) and verify IRSB output

### 2. Bochs Integration Architecture
- Design `IEmulator` / `ITraceProducer` interface
- Add Bochs as FetchContent/prebuilt dependency (LGPL v2.1)
- `BochsExecutor` implementation:
  - VM lifecycle management (boot, snapshot, restore)
  - Instrumentation hooks (`bx_instr_before_execution`)
  - Breakpoint-driven execution (execute only target function)
  - Yield `TraceRecord`s compatible with existing `HybridAnalysisPass`
- Snapshot format: CPU state + memory regions + device state

### 3. ConstantFoldingPass (VEX IR-based)
- Lift basic blocks to VEX IR
- Evaluate constant arithmetic operations (Add32, Sub32, etc. with constant operands)
- Propagate constants through temporaries
- Simplify identities (xor tmp, tmp → 0; sub tmp, tmp → 0; and tmp, 0xFFFF → zero-extend)

### 4. DeadCodeEliminationPass (VEX IR-based)
- Identify WrTmp/Put assignments that are never read (dead temporaries)
- Remove unreachable basic blocks
- Remove dead Store operations

### 5. DeobfuscationFlattenPass (VEX IR-based)
- Pattern-match dispatcher state variable in VEX IR
- Reconstruct original control flow from flattened switch-like state machines
- Write recovered CFG back to BasicBlock/Function structure

### 6. UnicornExecutor (Optional Accelerator)
- Encapsulate binspektor prototype as `UnicornExecutor` implementing `IEmulator`
- Syscall service layer: table-driven NT syscall handlers (~25-30 common)
- Only for simple micro-execution (arithmetic predicates, short code regions)
- Falls back to Bochs for unhandled cases (future)

## Key Files Recently Modified

- `ReWizardLib/include/ReWizard/Analysis/Passes/BasePass.hpp` (added Dependencies())
- `ReWizardLib/source/Analysis/PassManager.cpp` (topological sort)
- `ReWizardLib/include/ReWizard/Memory/MemoryMapper.h` (new)
- `ReWizardLib/include/ReWizard/Memory/Win32MemoryMapper.h` (new)
- `ReWizardLib/source/Memory/Win32MemoryMapper.cpp` (new)
- `ReWizardLib/include/ReWizard/Memory/PosixMemoryMapper.h` (new)
- `ReWizardLib/source/Memory/PosixMemoryMapper.cpp` (new)
- `ReWizardLib/include/ReWizard/FileLoader/FileLoader.h` (MemoryMapper integration)
- `ReWizardLib/source/FileLoader/FileLoader.cpp` (uses MemoryMapper, no direct Win32 calls)
- `ReWizardLib/include/ReWizard/Hybrid/SimpleTraceReader.h` (PC index)
- `ReWizardLib/source/Hybrid/SimpleTraceReader.cpp` (O(1) GetRecordsForPC)

## Build Reminders

- Always use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && <command>"`
- Boost is at `Z:/boost/boost_1_85_0`
- Commit small, focused changes only
- Test before every commit