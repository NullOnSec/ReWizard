# ReWizard Work Plan

Phased roadmap for building out ReWizard from its current state. Each phase produces a working, testable increment.

---

## Phase 0 — Stabilization & Testing Infrastructure ✅

**Goal:** Fix known bugs, establish testing, make the codebase ready for new development.

### 0.1 Fix Win32InternalTypeshpp
- Remove the duplicate definitions (lines 343-667) that exist outside the `#ifndef` guard and `namespace ReWizard`.
- Keep only the single copy inside the guard.

### 0.2 Fix AnalysisManager const_cast UB
- Remove the `AnalysisManager(const std::unique_ptr<AnalysisContext>&)` constructor that does `const_cast + std::move`.
- Keep only the rvalue-reference overload.

### 0.3 Fix PassManager thread safety
- Replace detached thread + promise/future with `std::async` or sequential execution.
- For now, `RunAllAsync` should just run passes sequentially in the calling thread (the async adds no real parallelism benefit currently since passes depend on each other's data).

### 0.4 Fix CMake WHOLEARCHIVE
- Replace the per-`.obj` WHOLEARCHIVE hack with a proper approach: wrap `.obj` groups or use a linker script, or simply rely on the auto-registration to ensure symbols are pulled in (the `PassProvider.cpp` explicit include already does this).

### 0.5 Remove hardcoded CLI path
- Add CLI argument parsing (start with a simple positional arg for the target path).
- Add `--help`, `--verbose`, `--output` flags as stubs for future use.

### 0.6 Add test framework
- Add a test target (Google Test via FetchContent) to the CMake build.
- Write initial tests:
  - `FileLoader` — load a minimal PE binary, verify mapping.
  - `Disassembler` — decode known-instruction byte sequences, verify mnemonic/operand output.
  - `StaticControlFlowRebuilder` — run against a small test binary, verify function count and call sites.
- Create `tests/` directory with a `CMakeLists.txt` and test sources.

### 0.7 Remove dead Unicorn link from CLI
- Remove `#include <unicorn/unicorn.h>` from `main.cpp`.
- Remove `unicorn` from `ReWizardCLI` link deps (keep in `ReWizardLib` for future use).

### 0.8 Fix O(n) function lookup
- Add an interval map (e.g., `std::map<uintptr_t, Function*>` keyed by start address) to `Module` for `GetFunctionForAddress`.
- Keep the vector for iteration order but use the map for lookups.

---

## Phase 1 — Import/Export Table Analysis ✅

**Goal:** Enrich the `Module` with symbol information from the binary's import/export tables, enabling named call targets and cross-references.

### 1.1 Add `SymbolTable` unit
- New `Analysis/Units/SymbolTable.h|.cpp`.
- Holds imported symbols (name, DLL, address), exported symbols, and reloc entries.
- Populated from LIEF's import/export APIs.

### 1.2 Add `ImportAnalysisPass`
- New `Analysis/Passes/ImportAnalysisPass.h|.cpp`.
- `Type = PEPass | ELFPass | MachOPass` depending on binary format.
- Reads LIEF imports/exports and fills `SymbolTable`.
- Resolves indirect call targets where possible (e.g., IAT entries with known DLL!API names).

### 1.3 Integrate symbol names into disassembly output
- `Function::GetDisassembly()` should annotate call targets with symbol names when available.

### 1.4 Add relocation processing to FileLoader
- For PE binaries where `VirtualAlloc` returns a different base than the preferred imagebase, apply base relocations so absolute addresses in instructions are correct.

### 1.5 Tests
- Test `SymbolTable` population against known binaries.
- Test relocation fixups by mapping a PE at a non-preferred base and verifying corrected addresses.

---

## Phase 2 — Improved Static Analysis ✅

**Goal:** Move beyond linear sweep to recursive descent, improve CFG accuracy, and produce useful output.

### 2.1 Recursive Descent Disassembly
- Enhance `StaticControlFlowRebuilder` (or add a new pass) to perform proper recursive descent:
  - Start from entrypoint + all exported symbols + all IAT targets.
  - Follow direct calls and branches.
  - Mark unreachable code regions for heuristic scan (linear sweep fallback).

### 2.2 Data Flow Analysis Pass
- New `DataFlowAnalysisPass` (GenericPass).
- Track register definitions and uses within basic blocks.
- Resolve indirect calls through register/stack tracking (e.g., `call rax` where `rax` was loaded from known address).

### 2.3 Abstract Interpretation Pass
- New `AbstractInterpretationPass` (GenericPass).
- Implement interval/domain analysis for register values.
- Use to resolve indirect jumps/calls and detect opaque predicates.
- Replace the current heuristic (`>30% stack ops → hybrid`) with sound analysis.

### 2.4 Output Generation
- Add `AnalysisResult` class to serialize analysis output:
  - JSON format: functions, basic blocks, CFG edges, call sites, symbol names.
  - GraphViz DOT export for CFG visualization.
  - Raw disassembly text export.
- Wire into CLI with `--output <path>` and `--format <json|dot|text>` flags.

### 2.5 Tests
- Test recursive descent against crafted byte sequences with known control flow.
- Test data flow analysis on small functions with indirect calls.
- Test abstract interpretation on opaque predicate patterns.

---

## Phase 3 — Hybrid Analysis

**Goal:** Implement dynamic analysis using execution traces to resolve targets that static analysis cannot. Bochs is the primary backend (full-system emulation); Unicorn is an optional accelerator for simple micro-execution.

### 3.1 Trace Infrastructure ✅
- `Hybrid/TraceRecord.h` — PC, registers, memory accesses.
- `ITraceReader.h` — abstract interface for trace consumers.
- `SimpleTraceReader` — JSON trace file reader with O(1) PC lookup.
- Pass dependency system with topological sort (fixes critical bug where 4/6 passes silently no-op).

### 3.2 Platform Abstraction ✅
- `MemoryMapper` interface extracts platform-specific memory management from `FileLoader`.
- `Win32MemoryMapper` (VirtualAlloc/VirtualFree) and `PosixMemoryMapper` (mmap/munmap).
- Unblocks Linux/macOS compilation.

### 3.3 HybridAnalysisPass ✅
- New `Analysis/Passes/HybridAnalysisPass.h|.cpp` (GenericPass).
- Consumes trace data from `IEmulator` / `ITraceReader`.
- Resolves indirect call/jump targets observed at runtime.
- Updates `Function::callSites_` with resolved targets.
- Removes opaque predicate markers from functions proven non-opaque by trace.
- Marks functions that were touched by trace as "hybrid-verified".

### 3.4 Hybrid/Static Iteration ✅
- After `HybridAnalysisPass` resolves indirect targets, re-run `StaticControlFlowRebuilder` on newly reachable code.
- Pass dependency system ensures correct execution order.

### 3.5 Bochs Integration
- Add Bochs as a FetchContent or prebuilt dependency (LGPL v2.1).
- Design `IEmulator` / `ITraceProducer` interface:
  - `Boot()` — start VM
  - `Snapshot()` / `Restore()` — save/load full VM state (CPU + memory + devices)
  - `ExecuteFunction()` — run target function with instrumentation
  - `GetTraceRecords()` — yield recorded instructions
- `BochsExecutor` implementation using Bochs instrumentation callbacks (`bx_instr_before_execution`).
- Snapshot strategy: boot once → save state → restore per analysis session.

### 3.6 UnicornExecutor (Optional Accelerator)
- Encapsulate binspektor prototype as `UnicornExecutor` implementing `IEmulator`.
- Syscall service layer: table-driven NT syscall handlers (~25-30 common calls).
- Only for simple micro-execution (arithmetic predicates, short code regions).
- Falls back to `BochsExecutor` for unhandled cases.

### 3.7 Tests
- Ship a small trace recording fixture.
- Test that `HybridAnalysisPass` correctly resolves indirect calls from trace data.
- Test Bochs snapshot save/restore roundtrip.

---

## Phase 4 — Deobfuscation (LLVM IR-Based)

**Goal:** Use LLVM IR as the single intermediate representation for deobfuscation. remill (Trail of Bits) lifts x86 binary bytes → LLVM IR. LLVM's optimizer passes provide constant folding (SCCP), dead code elimination (DCE), CFG simplification, and global value numbering (GVN). The X86 backend emits correct machine code for binary patching. One IR, one toolchain.

### 4.0 LLVM & remill Integration
- Add LLVM as a CMake FetchContent dependency with minimal configuration:
  - `-DLLVM_TARGETS_TO_BUILD=X86` (only x86/AMD64 target)
  - `-DLLVM_ENABLE_PROJECTS=""` (no Clang, no extra tools)
  - `-DLLVM_BUILD_TOOLS=OFF`, `-DLLVM_BUILD_EXAMPLES=OFF`, `-DLLVM_BUILD_TESTS=OFF`
- Add remill as a CMake FetchContent dependency (Trail of Bits, BSD-2-Clause-ish).
- Create `IR/Lifter.h|.cpp` — wraps remill to lift raw bytes at address → `llvm::Function` (LLVM IR).
- Create `IR/IRBlock.h` — C++ wrapper around LLVM IR types for ReWizard pass code (Value, Instruction, BasicBlock, Function).
- Integrate with `BasicBlock`: each BB holds an optional `llvm::BasicBlock*` for its lifted IR.
- Test: lift known x86 byte sequences (nop, mov, add, jmp) and verify LLVM IR output.

### 4.1 Opaque Predicate Elimination ✅
- `OpaquePredicatePass` (GenericPass).
- Uses `AbstractInterpretationPass` results to identify always-true / always-false branches.
- Rewrites CFG: removes dead branches, merges basic blocks.

### 4.2 Control Flow Flattening Recovery
- `DeobfuscationFlattenPass` (GenericPass).
- Lift flattened basic blocks to LLVM IR.
- Pattern-match dispatcher state variables in LLVM IR (phi nodes with state variable → switch-like structure).
- Reconstruct original control flow from IR-level analysis.
- Write recovered CFG back to `BasicBlock`/`Function` structure.
- For binary patching: lower reconstructed blocks through LLVM X86 backend → encoded bytes.

### 4.3 Dead Code Elimination
- `DeadCodeEliminationPass` (GenericPass).
- Operates on LLVM IR: use LLVM's built-in DCE and AggressiveDCE passes.
- Removes unreachable basic blocks (confirmed by abstract interpretation or hybrid trace).
- Removes dead stores (Store to address that is never read before next Store or function exit).
- Can also operate on ReWizard's `BasicBlock`/`Function` structure for passes that don't need IR.

### 4.4 Constant Folding & Simplification
- `ConstantFoldingPass` (GenericPass).
- Operates on LLVM IR: use LLVM's built-in SCCP (Sparse Conditional Constant Propagation) pass.
- Simplify arithmetic identities (e.g., `xor rax, rax` → 0 in LLVM IR).
- Propagate constants through LLVM IR (SCCP handles this automatically).
- For binary patching: lower simplified blocks through LLVM X86 backend → encoded bytes.

### 4.5 Binary Patching & Code Emission
- Create `Emission/MCEmitter.h|.cpp` — wraps LLVM MC layer to emit x86 bytes from LLVM IR functions.
- Create `Emission/Patcher.h|.cpp` — writes encoded bytes back into the loaded binary at specified offsets.
- Handle code size differences: when emitted code is shorter than original, pad with NOPs; when longer, use detour/trampoline.
- Optional: produce a patched binary file or in-place modification.
- Test: emit known instructions and verify byte output matches expected encodings.

### 4.6 Tests
- Craft obfuscated test binaries (opaque predicates, flattened CFG).
- Verify each deobfuscation pass produces expected simplified LLVM IR.
- Test constant folding on known arithmetic identities.
- Test full round-trip: lift → transform → emit → verify binary runs correctly.

---

## Phase 5 — Extended Features

**Goal:** Polish, performance, and advanced capabilities.

### 5.1 Multi-Architecture Support
- VEX IR already supports ARM, MIPS, PPC in addition to x86.
- Add `FileLoader` format detection for ELF and MachO.
- Add ELF/MachO-specific passes (`ELFImportAnalysisPass`, etc.).
- `Disassembler` abstraction: Zydis for x86/x86_64, Capstone for ARM/MIPS (or VEX native disassembly).

### 5.2 Kernel Driver Analysis
- Extend `FileLoader` to handle kernel-mode PE (no relocations, different section attributes).
- Add `KernelAnalysisPass` that sets up kernel execution context in Bochs.

### 5.3 Performance Optimizations
- Replace `std::map<uintptr_t, unique_ptr<ExtendedInstruction>>` with `std::unordered_map` or interval tree.
- Batch instruction decoding instead of single-instruction calls.
- Parallel function disassembly where data permits.

---

## Phase 6 — Analysis Database & Interactive UI

**Goal:** Persistent analysis sessions with an IDA Pro-like interactive experience. The database stores all analysis results and user annotations, enabling incremental re-analysis and sharing. The UI provides multi-view navigation of the binary.

### 6.1 Analysis Database Core
- Design `AnalysisDatabase` class: persistent storage for all analysis artifacts.
- Schema design: functions (address, size, name, type, convention), basic blocks (start, end, successors, predecessors), instructions (address, bytes, mnemonic, operands), symbols (address, name, type, source), cross-references (from, to, type: call/data/jump).
- Storage backend: SQLite (portable, queryable, no server process needed).
- Incremental save: database updated on each pass completion, not rebuilt from scratch.
- Headless mode: all analysis runs without UI, database is the single source of truth.

### 6.2 Cross-Reference (Xref) System
- Build bidirectional xref map from analysis results: call xrefs (function calls), data xrefs (memory reads/writes), jump xrefs (branch targets).
- `XrefManager` — stores from→to and to→from mappings for O(1) lookup.
- Populate from: ImportAnalysisPass (IAT entries), StaticControlFlowRebuilder (call/jump targets), DataFlowAnalysisPass (resolved indirects), HybridAnalysisPass (trace-verified targets).
- Expose via database queries: "who calls this function?", "what reads this address?".

### 6.3 Symbol & Annotation Persistence
- `SymbolManager` — user-defined names, typed variables, function signatures, comments.
- Rename symbols: override auto-generated names (sub_401000 → main).
- Function type annotations: specify calling convention, return type, parameter types.
- Inline comments: attach user notes to any address.
- Type system: structs, enums, typedefs with member layout and size information.
- All annotations stored in database, survive re-analysis passes.

### 6.4 Interactive Disassembly View
- Dear ImGui-based GUI (cross-platform: Windows, Linux, macOS via GLFW/GL3W or SDL).
- Disassembly panel: scrollable instruction list with address, bytes, mnemonic, operands.
- Symbol resolution: show names instead of raw addresses where available.
- Inline xref counts: `[Xrefs: 3]` annotations on call/jump targets.
- User interaction: double-click to follow address, right-click for context menu (rename, comment, xrefs).
- Color coding: highlighted registers, immediate constants, branch conditions.

### 6.5 Graph & Hex Views
- Graph view: Boost.Graph-derived CFG visualization with interactive node/edge navigation.
  - Zoom, pan, minimap overview.
  - Click node to focus, double-click to enter function, Escape to return.
  - Highlight: current path, loop back-edges, opaque predicate branches (dead edges dimmed).
  - Sync selection with disassembly view (click in graph → scroll to address in disasm).
- Hex view: raw bytes with decoded instruction overlay.
  - Column-aligned hex dump with ASCII representation.
  - Highlight modified bytes (relocations, deobfuscation patches).
  - Select bytes → show decoded instruction in status bar.

### 6.6 Function List & Search
- Searchable/filterable function table: name, address, size, type, # xrefs.
- Global search: search by address, symbol name, string constant, byte pattern.
- Bookmarks: save/restore navigation positions.
- Cross-reference panel: show all callers (call xrefs) and callees for selected function.

### 6.7 Console & Scripting API
- Command console: type commands to trigger passes, navigate, query database.
- Scripting API: expose `AnalysisContext`, `Module`, `Function`, `BasicBlock`, `SymbolManager`, `XrefManager` to a scripting language (Lua or Python via embedded interpreter).
- Batch mode: run analysis headlessly, export results to database, then open in GUI.
- Plugin system: load custom analysis passes as shared libraries via `.dll`/`.so`.

---

## Dependency Notes

- **LLVM** (v19+, X86 target only) is the single IR for deobfuscation and code emission. Only the MC layer and X86 target are linked — no Clang, no optimizer pipeline, no linker. Built with `-DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS=""`. Apache-2.0 license with LLVM exceptions. ~2-3GB built, ~30-80MB linked binary.
- **remill** (Trail of Bits) lifts x86/x86_64 binary bytes → LLVM IR. Production-grade, maintained, used by McSema2. BSD-2-Clause-ish license.
- **Bochs** will be integrated as a FetchContent dependency or linked as a prebuilt library (LGPL v2.1). Required for Phase 3 hybrid analysis.
- **Unicorn** remains linked in `ReWizardLib` for Phase 3.6 optional micro-execution accelerator.
- **Dear ImGui** will be integrated for Phase 6 interactive UI. Cross-platform (Windows/Linux/macOS), minimal dependencies, immediate-mode GUI suitable for custom analysis views.
- **SQLite** will be integrated for Phase 6 analysis database. Portable, serverless, queryable, zero-configuration.
- **Google Test** added via FetchContent for the test framework.
- All dependencies must be downloaded to `Z:` (not `C:`) per project policy.
- **PANDAS** is explicitly **not used** — Linux-only host, cannot run on Windows.

## Architectural Decisions

| Decision | Rationale |
|----------|-----------|
| Pass-based pipeline | Extensible, allows re-running passes after new info is discovered |
| Bochs primary backend | Full-system emulation — no API stubs needed, works on all host platforms. Snapshot mitigates boot time. |
| Unicorn optional accelerator | Lightweight micro-execution for simple arithmetic predicates. Not the foundation — correctness from Bochs. |
| No PANDAS | Linux-only host, cannot run on Windows. Cross-platform is a hard requirement. |
| VEX IR removed | VEX has no code generation backend. Cannot round-trip for binary patching. Using two IRs (VEX + LLVM) adds unnecessary complexity. |
| LLVM IR for deobfuscation and code emission | One IR, one toolchain. remill lifts x86→LLVM IR, LLVM optimizer passes handle constant folding/DCE/CFG simplification, X86 backend emits code. Battle-tested and production-grade. |
| remill as binary lifter | Maintained by Trail of Bits, used in McSema2, BSD-2-Clause-ish. No custom lifters — we use existing, maintained tools. |
| Minimal LLVM integration | Only X86 target + MC layer. No Clang, no optimizer pipeline (beyond what we use), no linker. `-DLLVM_TARGETS_TO_BUILD=X86`. ~30-80MB binary size acceptable for a reversing suite. |
| No custom x86 encoder | LLVM MC handles x86 encoding correctly (prefixes, ModRM, VEX/EVEX, REX.W). Writing our own would be a bug source. |
| Analysis database (SQLite) | Persistent storage of all analysis results and user annotations. Enables incremental re-analysis, session save/restore, and headless mode. SQLite is portable, serverless, and queryable. |
| Dear ImGui for UI | Cross-platform, minimal dependencies, immediate-mode GUI. Suitable for custom disassembly/graph/hex views. Allows fast iteration on UI without build-time overhead. |
| IDA Pro-like interaction model | Analysis database is source of truth. UI is a view onto the database. User annotations (renames, comments, types) survive re-analysis. Xrefs are bidirectional and queryable. |
| Boost.Graph for CFG | Already in use, well-tested, provides GraphViz output out of the box |
| Zydis for disassembly | Best-in-class x86 decoder, v4 supports all modern ISA extensions |
| Static auto-registration | Passes self-register via `PassRegistrar<T>` — no manual registry needed, just include the header |
| Pass dependency system | `Dependencies()` declarations with topological sort. Fixes critical bug where 4/6 passes silently no-op. |
| MemoryMapper abstraction | Platform-specific memory management isolated behind interface. Unblocks Linux/macOS compilation. |