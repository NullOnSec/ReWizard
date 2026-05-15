# ReWizard Work Plan

Phased roadmap for building out ReWizard from its current state. Each phase produces a working, testable increment.

---

## Phase 0 — Stabilization & Testing Infrastructure

**Goal:** Fix known bugs, establish testing, make the codebase ready for new development.

### 0.1 Fix Win32InternalTypes.hpp
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
- Remove `unicorn` from `ReWizardCLI` link deps (keep in `ReWizardLib` for now since it will be needed later).

### 0.8 Fix O(n) function lookup
- Add an interval map (e.g., `std::map<uintptr_t, Function*>` keyed by start address) to `Module` for `GetFunctionForAddress`.
- Keep the vector for iteration order but use the map for lookups.

---

## Phase 1 — Import/Export Table Analysis

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

## Phase 2 — Improved Static Analysis

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

## Phase 3 — Hybrid Analysis with PANDAS

**Goal:** Implement dynamic analysis using PANDAS (Platform for Architecture-Neutral Dynamic Analysis) to replay execution traces and resolve targets that static analysis cannot.

### 3.1 PANDAS Integration Layer
- Add PANDAS as a git submodule or FetchContent dependency.
- Create `Hybrid/PANDASRunner.h|.cpp` — wrapper to:
  - Load a PANDAS replay recording.
  - Extract instruction-level trace (PC, registers, memory accesses).
  - Expose trace data to the analysis pipeline.

### 3.2 Trace Recording Workflow
- Document the workflow for creating PANDAS replays:
  - Boot a Windows VM with PANDAS.
  - Load ntoskrnl using PANDAS OS introspection plugins.
  - Use the NT loader to launch the target user-mode process.
  - Record execution with `begin_record` / `end_record`.

### 3.3 HybridAnalysisPass
- New `Analysis/Passes/HybridAnalysisPass.h|.cpp` (GenericPass).
- Consumes PANDAS trace data.
- Resolves indirect call/jump targets observed at runtime.
- Updates `Function::callSites_` with resolved targets.
- Removes opaque predicate markers from functions proven non-opaque by trace.
- Marks functions that were touched by trace as "hybrid-verified".

### 3.4 Hybrid/Static Iteration
- After `HybridAnalysisPass` resolves indirect targets, re-run `StaticControlFlowRebuilder` on newly reachable code.
- Add pass dependency tracking to `PassManager` so hybrid runs after static.

### 3.5 Tests
- Ship a small PANDAS recording fixture (or scripts to generate one).
- Test that `HybridAnalysisPass` correctly resolves indirect calls from trace data.

---

## Phase 4 — Deobfuscation

**Goal:** Use accumulated analysis data to deobfuscate binary code.

### 4.1 Opaque Predicate Elimination
- `OpaquePredicatePass` (GenericPass).
- Uses `AbstractInterpretationPass` results to identify always-true / always-false branches.
- Rewrites CFG: removes dead branches, merges basic blocks.

### 4.2 Control Flow Flattening Recovery
- `DeobfuscationFlattenPass` (GenericPass).
- Pattern-matches control-flow flattening dispatcher structures (switch-based state machines).
- Reconstructs original control flow.

### 4.3 Dead Code Elimination
- `DeadCodeEliminationPass` (GenericPass).
- Removes unreachable basic blocks (confirmed by abstract interpretation or hybrid trace).
- Removes dead stores and unused assignments.

### 4.4 Constant Folding & Simplification
- `ConstantFoldingPass` (GenericPass).
- Evaluates constant expressions at analysis time.
- Simplifies arithmetic identities (e.g., `xor rax, rax` → 0).

### 4.5 Output Reconstructed Binary
- Optional: ability to write simplified/deobfuscated code back to a binary or IR representation.

### 4.6 Tests
- Craft obfuscated test binaries (opaque predicates, flattened CFG).
- Verify each deobfuscation pass produces the expected simplified output.

---

## Phase 5 — Extended Features

**Goal:** Polish, performance, and advanced capabilities.

### 5.1 Kernel Driver Analysis
- Extend `FileLoader` to handle kernel-mode PE (no relocations, different section attributes).
- Add `KernelAnalysisPass` that sets up kernel execution context in PANDAS.

### 5.2 Multi-format Improvements
- Improve ELF and MachO import/export handling in `ImportAnalysisPass`.
- Add format-specific passes for each binary type.

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

- **PANDAS** will be integrated as a git submodule pointing to `https://github.com/panda-re/panda`. Build requires QEMU; only needed for hybrid analysis phases.
- **Google Test** will be added via FetchContent for the test framework.
- All dependencies must be downloaded to `Z:` (not `C:`) per project policy.
- Unicorn remains linked in `ReWizardLib` but is not used in any current pass; it may be removed in favor of PANDAS or kept as a lightweight inline emulator for future micro-execution passes.

## Architectural Decisions

| Decision | Rationale |
|----------|-----------|
| Pass-based pipeline | Extensible, allows re-running passes after new info is discovered |
| PANDAS over Unicorn | PANDAS provides full OS emulation + replay; Unicorn only emulates bare-metal CPU and requires manual environment setup |
| Boost.Graph for CFG | Already in use, well-tested, provides GraphViz output out of the box |
| Zydis for disassembly | Best-in-class x86 decoder, v4 supports all modern ISA extensions |
| Static auto-registration | Passes self-register via `PassRegistrar<T>` — no manual registry needed, just include the header |