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

### 0.7 Remove Unicorn dependency ✅
- Unicorn was removed entirely — Bochs is the sole emulation backend.
- Static analysis + remill-based IR lifting handles cases that previously required Unicorn micro-execution.

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

**Goal:** Implement dynamic analysis using execution traces to resolve targets that static analysis cannot. Bochs is the sole emulation backend (full-system emulation).

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

### 3.5 Bochs Integration ✅
- `IEmulator` / `ITraceProducer` interface designed and implemented.
- `BochsExecutor` class created with full implementation:
  - PE parsing via LIEF
  - Memory mapping for kernel images
  - CPU state initialization (x86-64 registers, CR0/CR3/CR4)
  - Register get/set API
- **Bochs 3.0 built from source** with MSVC v143 (VS2022)
  - Instrumentation enabled (`BX_INSTRUMENTATION=1`)
  - Custom ReWizard instrumentation module
  - All libraries linked into `ReWizardLib`
- Windows system binaries gathered from licensed host install
- `scripts/gather-windows-binaries.ps1` for reproducible setup
- Snapshot strategy: boot once → save state → restore per analysis session.

### 3.6 BochsExecutor (Emulation Backend)
- `BochsExecutor` implementing `IEmulator`.
- Syscall service layer: table-driven NT syscall handlers (~25-30 common calls).
- Snapshot strategy: boot once → save state → restore per analysis session.

### 3.7 Tests
- Ship a small trace recording fixture.
- Test that `HybridAnalysisPass` correctly resolves indirect calls from trace data.
- Test Bochs snapshot save/restore roundtrip.

---

## Phase 4 — Deobfuscation (LLVM IR-Based)

**Goal:** Use LLVM IR as the single intermediate representation for deobfuscation. remill (Trail of Bits) lifts x86 binary bytes → LLVM IR with comprehensive instruction semantics (x86, x86_64, AVX, AVX512, X87, MMX, SSE). LLVM's optimizer passes provide constant folding (SCCP), dead code elimination (DCE), CFG simplification, and global value numbering (GVN). The X86 backend emits correct machine code for binary patching. One IR, one toolchain.

### 4.0 remill Integration (Replace Manual Lifter)

**Why remill instead of a manual lifter or RetDec extraction:**

The original implementation hand-lifted 6 instructions (MOV, ADD, SUB, XOR, NOP, RET) using Zydis + LLVM IRBuilder. This approach is fundamentally unmaintainable — x86/x86_64 has ~1500 instruction mnemonics, and manual per-instruction semantics will always be incomplete and buggy. We evaluated three options:

| Option | Verdict |
|--------|---------|
| **remill** (Trail of Bits) | **CHOSEN.** Apache-2.0, actively maintained (v6.0.1), LLVM 15+, comprehensive x86/x86_64/AArch64/SPARC semantics, designed as a library, used in production by McSema. |
| RetDec `capstone2llvmir` | **REJECTED.** Targets LLVM 8.0.0 (Avast fork with RetDec-specific LLVM patches). Porting to LLVM 19 would require forking and maintaining a custom LLVM fork. Project in limited maintenance mode. Uses Capstone (we use Zydis). Extraction possible but LLVM version mismatch is fatal. |
| Continue manual lifter | **REJECTED.** 6/~1500 mnemonics implemented. Instruction semantics are wrong (no flags, no memory model, no calling convention context). Fundamentally untenable for production use. |

#### 4.0a Build remill on Windows ✅

- Clone remill repository (v6.0.1).
- Build remill superbuild (`dependencies/` directory) with MSVC/clang-cl.
  - remill requires LLVM 15+ and Intel XED; both are fetched by the superbuild.
  - Windows build requires `clang-cl` compiler (MSVC clang frontend).
  - Configure: `-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl`.
  - Built remill against our existing LLVM 19 at `Z:/llvm-install` (`USE_EXTERNAL_LLVM=ON`).
  - Built `llvm-link` from our LLVM source and installed to `Z:/llvm-install/bin/`.
- Patched remill for x86-only build (our LLVM only has X86 target):
  - `CMakeLists.txt`: removed aarch64/arm/sparc LLVM components from `llvm_map_components_to_libnames`
  - `lib/Arch/CMakeLists.txt`: only `add_subdirectory(X86)`, only link `remill_arch_x86`
  - `lib/Arch/Arch.cpp`: `GetArchByName` and `AddressSize` only handle x86/AMD64 variants
  - `cmake/BCCompiler.cmake`: `find_program` fallback for `llvm-link`/`clang++` when CMake target missing
- Manually installed remill libraries/headers/cmake to `Z:/remill-install` (CMake install fails on sleigh `.sla` file bug).
- Created `scripts/build-remill.ps1` — idempotent build script with automatic patching.
- Validated: `remill-lift-19.exe` links and runs.

#### 4.0b Integrate remill into ReWizard CMake ✅

- Added remill as a manual CMake dependency (avoided `find_package(remill)` because remill's cmake config has a complex sleigh dependency chain).
- remill links against Intel XED, glog, gflags — all present in `Z:/remill-install`.
- Updated `ReWizardLib/CMakeLists.txt` to link remill libraries directly with full paths.
- LLVM version coordination: remill built against our LLVM 19 — no version mismatch.
- **Note**: remill's superbuild installs gtest 1.17.0 to `Z:/remill-install/include/gtest`. This shadows ReWizard's FetchContent gtest 1.14.0 if added as `PUBLIC` include. Fixed by adding remill include as `PRIVATE` to `ReWizardLib`.
- All 39 tests pass (2 skipped). `ReWizardLib.lib` and `ReWizardCli.exe` build and link successfully.

#### 4.0c Rewrite Lifter to use remill API ✅

- Replaced the manual Zydis+IRBuilder implementation in `IR/Lifter.cpp` with remill calls.
- remill's core API used:
  - `remill::Arch::Get(context, os, arch)` — get the architecture object.
  - `remill::LoadArchSemantics(arch)` — load semantics bitcode module (required for instruction semantics lookup).
  - `arch->DefineLiftedFunction(name, module)` — create a lifted function with the remill signature `(State*, PC, Memory*) -> Memory*`.
  - `arch->InitializeEmptyLiftedFunction(func)` — set up local variables (STATE, MEMORY, NEXT_PC, BRANCH_TAKEN, etc.).
  - `remill::InstructionLifter::LiftIntoBlock(inst, block, state_ptr)` — lift a decoded instruction into a basic block.
- The `Lifter` class interface (`LiftBasicBlock`, `LiftFunction`, `GetModule`, `GetContext`, `DumpModule`) stays the same — only the implementation changed.
- PC initialization: override the `NEXT_PC` store in `InitializeEmptyLiftedFunction` with the actual block start address so `LiftIntoBlock` computes correct instruction PCs.
- Control flow: after lifting all instructions in a block, add `ret mem_ptr` to terminate the function.
- Unsupported instructions: handled gracefully — log a warning and skip (remill's semantics module has `ISEL_UNSUPPORTED_INSTRUCTION` fallback).
- Manual lifter code kept as `#elif defined(REWIZARD_LLVM_ENABLED)` fallback for builds without remill.

#### 4.0d Adapt remill's memory and register model

- remill represents machine state as a `State` struct (all registers, flags, etc.) and uses memory access intrinsics (`__remill_read_memory_8/16/32/64`, `__remill_write_memory_8/16/32/64`).
- For static analysis, the memory intrinsics are opaque function calls in LLVM IR — the optimizer can still reason about them (constant propagation through memory addresses, DCE of unused loads).
- `ReWizardMemory` class (concrete memory interface for remill) is deferred until concrete execution is needed (Phase 3.5 Bochs integration or Phase 4.5 binary patching).
- Each lifted function operates on the `State` struct. For inter-function analysis, the optimizer sees the `State` as one large aggregate — SCCP and DCE can still eliminate dead register writes.
- For per-basic-block lifting (our use case), remill lifts each instruction into a call to its semantics function. Our `IRLiftingPass` calls remill per block, collecting the resulting LLVM IR into our shared module.

#### 4.0e Remove manual lifter code

- Manual lifter code (per-instruction switch-case for MOV/ADD/SUB/XOR/NOP/RET, `GetRegisterIndex()`, `GetRegisterSizeBits()`, alloca-based register array) moved behind `#elif defined(REWIZARD_LLVM_ENABLED)`.
- This code is deprecated but retained as a fallback for builds where remill is unavailable.
- The primary path (`#ifdef REWIZARD_REMILL_ENABLED`) uses remill exclusively.
- `IRLiftingPass.cpp` unchanged — it uses the `Lifter` class interface, which now dispatches to remill.

#### 4.0f Validate remill-based lifting ✅

- All 39 tests pass (2 skipped) — including `LifterTest.CreatesModuleAndContext`, `LifterTest.LiftBasicBlockReturnsSuccess`, `LifterTest.LiftMovImmProducesIR`, `LifterTest.DumpModuleProducesText`.
- CLI runs successfully on test binaries (`test_pe.exe`, `test_hello.exe`) — full pipeline: FileLoader → StaticControlFlowRebuilder → IRLiftingPass → ConstantFoldingPass.
- Verified lifted IR contains proper remill function signatures: `define internal ptr @bb_140001000(ptr noalias %state, i64 %program_counter, ptr noalias %memory)`.
- Optimization pipeline (SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE) runs successfully on remill-generated IR without crashes.
- Comprehensive instruction coverage: remill handles ~1500 x86/x86_64 mnemonics including integer ops, X87, MMX, SSE, AVX, AVX512 (vs. 6 mnemonics in manual lifter).

### 4.1 Opaque Predicate Elimination ✅
- `OpaquePredicatePass` (GenericPass).
- Uses `AbstractInterpretationPass` results to identify always-true / always-false branches.
- Rewrites CFG: removes dead branches, merges basic blocks.

### 4.2 Control Flow Flattening Recovery
- `DeobfuscationFlattenPass` (GenericPass).
- Lift flattened basic blocks to LLVM IR (now using remill-generated IR with full instruction semantics).
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
- Verify comprehensive remill lifting: test instructions that the manual lifter never handled (CALL, JMP, CMP, TEST, conditional branches, PUSH/POP, memory operations, LEA, IMUL, DIV, SHL/SHR, flag-setting instructions).
- Verify each deobfuscation pass produces expected simplified LLVM IR.
- Test constant folding on known arithmetic identities.
- Test full round-trip: lift → transform → emit → verify binary runs correctly.

---

## Phase 5 — Extended Features

**Goal:** Polish, performance, and advanced capabilities.

### 5.1 Multi-Architecture Support
- **Target priority:** x64 PE > x86 PE > Linux ELF > Mach-O. All test fixtures and primary development focus on x64 PE.
- remill already supports AArch64 and SPARC in addition to x86/x86_64 — lifting coverage extends automatically.
- Add `FileLoader` format detection for ELF and MachO.
- Add ELF/MachO-specific passes (`ELFImportAnalysisPass`, etc.).
- `Disassembler` abstraction: Zydis for x86/x86_64 (display/analysis), remill uses Intel XED internally (no conflict).

### 5.2 Kernel Driver Analysis
- Extend `FileLoader` to handle kernel-mode PE (no relocations, different section attributes).
- Add `KernelAnalysisPass` that sets up kernel execution context in Bochs.

### 5.3 Performance Optimizations
- Replace `std::map<uintptr_t, unique_ptr<ExtendedInstruction>>` with `std::unordered_map` or interval tree.
- Batch instruction decoding instead of single-instruction calls.
- Parallel function disassembly where data permits.

---

## Phase 6 — Analysis Database & Interactive UI (Primary Entry Point)

**Goal:** Persistent analysis sessions with an IDA Pro-like interactive experience. The UI is the **primary entry point** for ReWizard — users open binaries through the GUI, analysis runs in the background, and results are presented in interactive views. The CLI remains as a secondary headless/batch mode.

The database stores all analysis results and user annotations, enabling incremental re-analysis and sharing. The UI provides multi-view navigation of the binary.

### 6.1 Analysis Database Core ✅
- Design `AnalysisDatabase` class: persistent storage for all analysis artifacts.
- Schema design: functions (address, size, name, type, convention), basic blocks (start, end, successors, predecessors), instructions (address, bytes, mnemonic, operands), symbols (address, name, type, source), cross-references (from, to, type: call/data/jump).
- Storage backend: SQLite (portable, queryable, no server process needed). Added via FetchContent amalgamation.
- Incremental save: database updated on each pass completion, not rebuilt from scratch.
- Headless mode: all analysis runs without UI, database is the single source of truth.
- Implemented: Open/Close, CreateSchema, BeginTransaction/Commit/Rollback, SaveFunction(s), SaveBasicBlock(s), SaveSymbolTable, SaveXref, QueryFunctions, QueryFunctionByAddress, QueryXrefsTo/From, QueryAllXrefs, SaveModule.

### 6.2 Cross-Reference (Xref) System ✅
- Build bidirectional xref map from analysis results: call xrefs (function calls), data xrefs (memory reads/writes), jump xrefs (branch targets).
- `XrefManager` — stores from→to and to→from mappings for O(1) lookup via `unordered_multimap`.
- Populate from: ImportAnalysisPass (IAT entries), StaticControlFlowRebuilder (call/jump targets), DataFlowAnalysisPass (resolved indirects), HybridAnalysisPass (trace-verified targets).
- Expose via database queries: "who calls this function?", "what reads this address?".
- Implemented: AddXref, RemoveXref, GetXrefsTo/From, GetCallsTo/From, GetJumpsTo/From, GetDataRefsTo, SaveTo/LoadFrom AnalysisDatabase.
- `StaticControlFlowRebuilder` populates xrefs during CFG recovery for all direct/indirect calls and jumps.

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
- **remill** (Trail of Bits, Apache-2.0) lifts x86/x86_64 binary bytes → LLVM IR. Production-grade, actively maintained (v6.0.1), used by McSema. Provides comprehensive instruction semantics (integer, X87, MMX, SSE, AVX, AVX512). Replaces the manual Zydis+IRBuilder lifter entirely. Built via its own superbuild (fetches Intel XED, glog, gflags). Installed to `Z:/remill-install`.
- **Intel XED** (bundled with remill) provides instruction decoding for remill. Not a direct ReWizard dependency — used internally by remill. No need for a second decoder alongside Zydis (Zydis remains for our own disassembly pass; remill/XED handles lifting semantics).
- **Bochs** will be integrated as a FetchContent dependency or linked as a prebuilt library (LGPL v2.1). Required for Phase 3 hybrid analysis.
- **Bochs** is the sole emulation backend; no Unicorn.
- **Dear ImGui** will be integrated for Phase 6 interactive UI. Cross-platform (Windows/Linux/macOS via GLFW/GL3W or SDL), minimal dependencies, immediate-mode GUI suitable for custom analysis views.
- **SQLite** (amalgamation, public domain) integrated for Phase 6 analysis database. Portable, serverless, queryable, zero-configuration. Fetched via FetchContent from official sqlite.org amalgamation.
- **Google Test** added via FetchContent for the test framework.
- All dependencies must be downloaded to `Z:` (not `C:`) per project policy.
- **PANDAS** is explicitly **not used** — Linux-only host, cannot run on Windows.

### Lifter Evaluation (Why remill)

| Lifter | License | LLVM Version | ISA Coverage | Maintenance | Verdict |
|--------|---------|-------------|--------------|-------------|---------|
| **remill** | Apache-2.0 | 15+ | x86, x86_64, AArch64, SPARC | Active (Trail of Bits) | **CHOSEN** |
| RetDec `capstone2llvmir` | MIT | 8.0.0 (Avast fork + patches) | x86, x86_64, ARM, MIPS, PPC | Limited maintenance | REJECTED: LLVM 8 fork incompatible with LLVM 19 |
| Manual (Zydis+IRBuilder) | N/A | 19 | 6/~1500 mnemonics | Us | REJECTED: fundamentally incomplete |
| McSema | AGPL-3.0 | 9-11 | x86, x86_64, AArch64 | Archived (2022) | REJECTED: archived, uses remill internally |

## Architectural Decisions

| Decision | Rationale |
|----------|-----------|
| Pass-based pipeline | Extensible, allows re-running passes after new info is discovered |
| Bochs primary backend | Full-system emulation — no API stubs needed, works on all host platforms. Snapshot mitigates boot time. |
| Bochs emulation backend | Full-system emulation for dynamic analysis. Snapshot/restore for efficient re-analysis. |
| No PANDAS | Linux-only host, cannot run on Windows. Cross-platform is a hard requirement. |
| VEX IR removed | VEX has no code generation backend. Cannot round-trip for binary patching. Using two IRs (VEX + LLVM) adds unnecessary complexity. |
| LLVM IR for deobfuscation and code emission | One IR, one toolchain. remill lifts x86→LLVM IR, LLVM optimizer passes handle constant folding/DCE/CFG simplification, X86 backend emits code. Battle-tested and production-grade. |
| **remill as binary lifter** | **Replaces manual Zydis+IRBuilder lifter.** remill is maintained by Trail of Bits, used in production (McSema2), Apache-2.0 license. Provides comprehensive x86/x86_64 instruction semantics including AVX, AVX512, X87, MMX, SSE. No manual per-instruction lifter code — we use existing, maintained semantics. |
| No RetDec lifter extraction | RetDec's `capstone2llvmir` module targets LLVM 8.0.0 (Avast fork with RetDec-specific patches in LLVM internals). Porting to LLVM 19 would require maintaining a custom LLVM fork, which is unsustainable. remill works with upstream LLVM 15+. |
| Minimal LLVM integration | Only X86 target + MC layer. No Clang, no optimizer pipeline (beyond what we use), no linker. `-DLLVM_TARGETS_TO_BUILD=X86`. ~30-80MB binary size acceptable for a reversing suite. |
| No custom x86 encoder | LLVM MC handles x86 encoding correctly (prefixes, ModRM, VEX/EVEX, REX.W). Writing our own would be a bug source. |
| Zydis for disassembly, XED for lifting semantics | Zydis v4 remains our disassembler (best-in-class x86 decoder). remill uses Intel XED internally for instruction decoding during lifting. No conflict — Zydis handles our display/analysis pass, XED handles remill's semantics translation. |
| Analysis database (SQLite) | Persistent storage of all analysis results and user annotations. Enables incremental re-analysis, session save/restore, and headless mode. SQLite is portable, serverless, and queryable. |
| Dear ImGui for UI | Cross-platform, minimal dependencies, immediate-mode GUI. Suitable for custom disassembly/graph/hex views. Allows fast iteration on UI without build-time overhead. |
| IDA Pro-like interaction model | Analysis database is source of truth. UI is a view onto the database. User annotations (renames, comments, types) survive re-analysis. Xrefs are bidirectional and queryable. |
| Boost.Graph for CFG | Already in use, well-tested, provides GraphViz output out of the box |
| Static auto-registration | Passes self-register via `PassRegistrar<T>` — no manual registry needed, just include the header |
| Pass dependency system | `Dependencies()` declarations with topological sort. Fixes critical bug where 4/6 passes silently no-op. |
| MemoryMapper abstraction | Platform-specific memory management isolated behind interface. Unblocks Linux/macOS compilation. |