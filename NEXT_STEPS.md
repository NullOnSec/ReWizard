# Next Session Plan

Updated: 2026-05-15

## Architecture Decisions Updated

### Hybrid Engine: Bochs Primary, Unicorn Optional

- **PANDAS is OUT** — Linux-only host, requires QEMU build, cannot run on Windows
- **Bochs is IN as primary backend** — full-system emulation, no API stubs needed, works on Windows/Linux/macOS
- **Unicorn is IN as optional accelerator** — for simple micro-execution (arithmetic predicates), with syscall service layer for common NT calls. Never the foundation.
- **No Python scripts** — everything embedded in C++. No "record on Linux / analyze on Windows" workflow.
- **Bochs snapshot strategy** — boot once, save CPU+memory state, restore for each analysis session

### IR: LLVM IR (remill + LLVM optimizer + X86 backend)

- **Single IR, single toolchain** — LLVM IR for everything: lifting, analysis, optimization, code emission
- **remill** (Trail of Bits) lifts x86/x86_64 binary bytes → LLVM IR. Production-grade, maintained, used in McSema2.
- **LLVM optimizer passes** provide battle-tested constant propagation (SCCP), dead code elimination (DCE/ADCE), CFG simplification, and global value numbering (GVN) — exactly what deobfuscation needs
- **LLVM X86 backend** emits correct machine code for binary patching — handles x86 encoding complexity (prefixes, ModRM, VEX/EVEX, REX.W) so we don't have to write our own encoder
- **VEX IR rejected** — no code generation backend, can't round-trip for binary patching. Using two IRs adds unnecessary complexity.
- **Custom encoders rejected** — x86 encoding is notoriously complex, LLVM MC handles it correctly
- LLVM integrated minimally: `-DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS=""`. No Clang, no linker, no frontend. ~2-3GB built, ~30-80MB linked.

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

### 1. LLVM & remill Integration (Prerequisite for Phase 4 Deobfuscation)
- Add LLVM as CMake FetchContent dependency (minimal: X86 target only, no Clang)
  - `-DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS="" -DLLVM_BUILD_TOOLS=OFF`
- Add remill as CMake FetchContent dependency (Trail of Bits binary lifter)
- Create `IR/Lifter.h|.cpp` — wraps remill to lift raw bytes at address → `llvm::Function`
- Create `IR/IRBlock.h` — C++ wrapper around LLVM IR types for ReWizard pass code
- Integrate with `BasicBlock`: each BB holds optional `llvm::BasicBlock*` for its lifted IR
- Test: lift known x86 byte sequences (nop, mov, add, jmp) and verify LLVM IR output

### 2. Bochs Integration Architecture
- Design `IEmulator` / `ITraceProducer` interface
- Add Bochs as FetchContent/prebuilt dependency (LGPL v2.1)
- `BochsExecutor` implementation:
  - VM lifecycle management (boot, snapshot, restore)
  - Instrumentation hooks (`bx_instr_before_execution`)
  - Breakpoint-driven execution (execute only target function)
  - Yield `TraceRecord`s compatible with existing `HybridAnalysisPass`
- Snapshot format: CPU state + memory regions + device state

### 3. ConstantFoldingPass (LLVM IR-based)
- Lift basic blocks to LLVM IR via remill
- Use LLVM's built-in SCCP (Sparse Conditional Constant Propagation) pass
- Simplify arithmetic identities (xor rax, rax → 0, sub rax, rax → 0, etc.)
- For binary patching: lower simplified blocks through LLVM X86 backend

### 4. DeadCodeEliminationPass (LLVM IR-based)
- Use LLVM's built-in DCE and AggressiveDCE passes
- Remove unreachable basic blocks
- Remove dead Store operations
- Can also operate on ReWizard's BasicBlock/Function structure for non-IR passes

### 5. DeobfuscationFlattenPass (LLVM IR-based)
- Pattern-match dispatcher state variable in LLVM IR (phi nodes with state variable)
- Reconstruct original control flow from flattened switch-like state machines
- Write recovered CFG back to BasicBlock/Function structure
- For binary patching: lower reconstructed blocks through LLVM X86 backend

### 6. UnicornExecutor (Optional Accelerator)
- Encapsulate binspektor prototype as `UnicornExecutor` implementing `IEmulator`
- Syscall service layer: table-driven NT syscall handlers (~25-30 common)
- Only for simple micro-execution (arithmetic predicates, short code regions)
- Falls back to Bochs for unhandled cases (future)

## Backlog: Phase 6 — Analysis Database & Interactive UI

IDA Pro-like interactive analysis experience. Database is the single source of truth; UI is a view onto it.

### 6.1 Analysis Database Core (SQLite)
- `AnalysisDatabase` class: persistent storage for functions, BBs, instructions, symbols, xrefs, annotations
- SQLite backend: portable, serverless, queryable
- Incremental save: updated per-pass, not rebuilt from scratch
- Headless mode: analysis runs without UI, database is truth

### 6.2 Cross-Reference (Xref) System
- `XrefManager`: bidirectional from→to and to→from maps (call, data, jump xrefs)
- Populated from ImportAnalysisPass, StaticControlFlowRebuilder, DataFlowAnalysisPass, HybridAnalysisPass
- O(1) lookup: "who calls this?", "what reads this address?"

### 6.3 Symbol & Annotation Persistence
- `SymbolManager`: user renames, function types, calling conventions, inline comments
- Type system: structs, enums, typedefs with member layout
- All annotations stored in database, survive re-analysis

### 6.4 Interactive Disassembly View
- Dear ImGui-based GUI (cross-platform: Windows/Linux/macOS)
- Disassembly panel: address bytes mnemonic operands with symbol resolution
- Inline xref counts, color coding, right-click context menus
- Double-click follow address, rename, add comment

### 6.5 Graph & Hex Views
- Graph view: CFG visualization with zoom, pan, minimap, function entry/exit highlighting
- Hex view: raw bytes with decoded instruction overlay, relocation highlighting
- Synced selection: click in graph → scroll in disasm, and vice versa

### 6.6 Function List & Search
- Searchable/filterable function table
- Global search: address, symbol name, string constant, byte pattern
- Bookmarks: save/restore navigation positions

### 6.7 Console & Scripting API
- Command console: trigger passes, navigate, query database
- Scripting API: expose analysis objects to Lua or embedded Python
- Plugin system: load custom analysis passes as shared libraries

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