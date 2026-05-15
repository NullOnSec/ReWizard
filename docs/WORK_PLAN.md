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

## Phase 4 — Deobfuscation (IR-Based)

**Goal:** Use VEX IR to perform architecture-independent deobfuscation transformations. Operating on IR instead of raw x86 instructions makes passes sound, portable, and composable.

### 4.0 VEX IR Integration
- Add angr's `libvex` (from pyvex C core) as a CMake FetchContent dependency.
- Create `IR/VEXLifter.h|.cpp` — wraps `libvex` to lift raw bytes to `IRSB` (IR Super Block).
- Integrate with `BasicBlock` — each BB holds an optional `IRSB` for its lifted IR.
- Create `IR/IRBlock.h` — C++ wrapper around VEX IR types (IRStmt, IRExpr, IRType) for cleaner pass code.
- Test: lift known x86 byte sequences, verify IR output.

### 4.1 Opaque Predicate Elimination ✅
- `OpaquePredicatePass` (GenericPass).
- Uses `AbstractInterpretationPass` results to identify always-true / always-false branches.
- Rewrites CFG: removes dead branches, merges basic blocks.

### 4.2 Control Flow Flattening Recovery
- `DeobfuscationFlattenPass` (GenericPass).
- Lift flattened basic blocks to VEX IR.
- Pattern-match dispatcher state variables in VEX IR (WrTmp of state variable → Switch-like Exit structure).
- Reconstruct original control flow from IR-level analysis.
- Write recovered CFG back to `BasicBlock`/`Function` structure.

### 4.3 Dead Code Elimination
- `DeadCodeEliminationPass` (GenericPass).
- Operates on VEX IR: identify WrTmp/Put assignments that are never read.
- Removes unreachable basic blocks (confirmed by abstract interpretation or hybrid trace).
- Removes dead stores (Store to address that is never read before next Store or function exit).

### 4.4 Constant Folding & Simplification
- `ConstantFoldingPass` (GenericPass).
- Operates on VEX IR: evaluate constant arithmetic operations at analysis time.
- Simplify arithmetic identities (e.g., `xor rax, rax` → WrTmp(t0) = 0:I64).
- Propagate constants through VEX temporaries (constantargh-style propagation).
- This is sound because VEX makes all side-effects explicit.

### 4.5 IR → Native Writeback (Future)
- After IR-level transformations, write simplified IR back to native code.
- This is optional and can be deferred — the primary use case is analysis, not binary rewriting.
- If implemented, use VEX's built-in x86/AMD64 backend for IR → bytes.

### 4.6 Tests
- Craft obfuscated test binaries (opaque predicates, flattened CFG).
- Verify each deobfuscation pass produces the expected simplified VEX IR.
- Test constant folding on known arithmetic identities.

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

### 5.4 Interactive CLI / TUI
- Add a terminal UI (e.g., imTUI or similar) for:
  - Viewing functions list.
  - Navigating disassembly with cross-references.
  - Triggering re-analysis on demand.

### 5.5 Plugin System
- Expose a C API or plugin interface for custom passes.
- Allow loading external analysis passes as shared libraries.

---

## Dependency Notes

- **VEX IR** (from angr/pyvex) is the chosen intermediate representation for deobfuscation passes. BSD-2-Clause license. C library compiles with MSVC. Supports x86, AMD64, ARM, ARM64, MIPS, PPC. No custom lifters — we use angr's maintained `libvex` directly.
- **Bochs** will be integrated as a FetchContent dependency or linked as a prebuilt library (LGPL v2.1). Required for Phase 3 hybrid analysis.
- **Unicorn** remains linked in `ReWizardLib` for Phase 3.6 optional micro-execution accelerator.
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
| VEX IR for deobfuscation | Architecture-independent, maintained by angr, BSD-2-Clause, compiles with MSVC. Enables sound constant folding, dead code elimination, and CFF recovery on IR instead of raw x86. Multi-arch future-proof. |
| No LLVM IR | LLVM is too large/heavyweight for our needs. VEX is lighter, purpose-built for binary analysis, and already supports the architectures we need. |
| No custom lifters | We must use readily available, maintained lifters — not write our own. VEX/pyvex provides this. |
| Boost.Graph for CFG | Already in use, well-tested, provides GraphViz output out of the box |
| Zydis for disassembly | Best-in-class x86 decoder, v4 supports all modern ISA extensions |
| Static auto-registration | Passes self-register via `PassRegistrar<T>` — no manual registry needed, just include the header |
| Pass dependency system | `Dependencies()` declarations with topological sort. Fixes critical bug where 4/6 passes silently no-op. |
| MemoryMapper abstraction | Platform-specific memory management isolated behind interface. Unblocks Linux/macOS compilation. |