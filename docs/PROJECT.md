# ReWizard

Binary analysis framework for x86/x86_64 reverse engineering, built on a pass-based architecture. Designed for multi-architecture future support via VEX IR.

## Architecture

ReWizard follows a **pass-based analysis pipeline** (inspired by compiler IR passes). An `AnalysisManager` owns an `AnalysisContext` (holding the binary mapping, module, and disassembler) and an `AnalysisPassManager` that runs registered passes in dependency order (topological sort of `Dependencies()` declarations). Passes auto-register via `PassRegistrar<T>` with static initialization and are instantiated by `PassProvider::Init()`.

```
AnalysisManager
 ├── AnalysisContext
 │    ├── FileLoader      (LIEF parser + MemoryMapper memory mapping)
 │    ├── Module          (owns Functions → BasicBlocks → CFG)
 │    ├── Disassembler    (Zydis v4.1.0, thread-safe via Proxy/Mutex)
 │    └── visited addrs   (std::set<uintptr_t>)
 └── AnalysisPassManager
      └── PassProvider → topological sort → pass execution
```

### Data Model

- **Module** — top-level container; holds all `Function`s and the global instruction map (`std::map<uintptr_t, unique_ptr<ExtendedInstruction>>`).
- **Function** — boundaries (`start`/`end`/`lastInsnAddr`), `CallSite` set, `BasicBlock` collection, Boost.Graph CFG, opaque-predicate heuristic flag, trampoline flag, disassembly cache.
- **BasicBlock** — start/end addresses, successor/predecessor address lists, `Split()` for re-structuring, indirect-call/jump flags.
- **DecodedInstruction / ExtendedInstruction** — wraps Zydis decoded instruction + operands; `ExtendedInstruction` adds `IsIndirect` and `IndirectValue`.

### IR Layer (Phase 4+)

Deobfuscation uses a **split IR architecture**:

**VEX IR for analysis** — lifting raw bytes to IR for understanding semantics, detecting dead code, constant propagation, CFF pattern matching. VEX is the analysis IR: it makes all side-effects explicit, is architecture-independent, and angr maintains it.

**LLVM MC for code emission** — when deobfuscation needs to patch the binary (e.g., rewriting a flattened function), LLVM MC lowers the transformed code back to correct x86/x86_64 bytes. VEX has no code generation backend, so LLVM MC fills this role.

This split means we don't need the full LLVM toolchain — only the MC (Machine Code) layer and X86 target description. No Clang, no optimizer pipeline, no linker. Built with `-DLLVM_TARGETS_TO_BUILD=X86`, the resulting binary size is ~30-80MB, acceptable for a reversing suite.

```
Binary bytes ──(VEX)──► IRSB (analysis) ──(transform)──► Modified IRSB
                                                            │
                                      ┌─────────────────────┘
                                      ▼
                            Lower to LLVM MC ──(emit)──► Patched bytes
```

For analysis-only passes (constant folding detection, dead code identification, opaque predicate reasoning), VEX IR is sufficient and no code emission is needed. Only CFF unflattening and binary patching require the LLVM MC lowering path.

### Current Passes

| Pass                    | Type          | Dependencies              | Status |
|-------------------------|---------------|---------------------------|--------|
| ImportAnalysisPass      | PEPass        | (none)                    | Working |
| StaticControlFlowRebuilder | GenericPass | ImportAnalysisPass        | Working |
| DataFlowAnalysisPass    | GenericPass   | StaticControlFlowRebuilder | Working |
| AbstractInterpretationPass | GenericPass | StaticControlFlowRebuilder | Working |
| OpaquePredicatePass     | GenericPass   | AbstractInterpretationPass | Working |
| HybridAnalysisPass      | GenericPass   | StaticControlFlowRebuilder, AbstractInterpretationPass, OpaquePredicatePass | Working |

### Hybrid Analysis Architecture

```
IEmulator (interface)
├── BochsExecutor      (primary) — full-system emulation, snapshot support
├── UnicornExecutor    (optional) — micro-execution fast-path
└── SimpleTraceReader  (file-based) — JSON trace replay

HybridAnalysisPass ← ITraceReader ← IEmulator
```

**Bochs** is the primary backend: boot a Windows VM once, snapshot CPU+memory state, restore per analysis session. No API stubs needed.

**Unicorn** is the optional accelerator: simple arithmetic predicates and short code regions. Falls back to Bochs for complex cases.

### Third-Party Dependencies

| Library  | Version / Branch      | Purpose                                  |
|----------|-----------------------|------------------------------------------|
| Zydis    | v4.1.0                | x86/x86_64 instruction decode & format   |
| LIEF     | extended_build_patch  | PE/ELF/MachO binary parsing              |
| Unicorn  | v2.1.0 (x86 only)     | CPU micro-execution (optional accelerator) |
| Bochs    | (TBD)                 | Full-system emulation (primary backend)   |
| VEX IR   | angr/pyvex (C core)  | Analysis IR: lifting bytes → IR for deobfuscation |
| LLVM MC  | v19+ (X86 target only) | Code emission: lowering transformed IR → x86 bytes for binary patching |
| Bochs    | (TBD)                 | Full-system emulation (primary backend)   |
| Boost    | 1.85.0                | Graph (adjacency_list, GraphViz output)   |
| spdlog   | (via LIEF)            | Logging                                  |

**Dependency policy:** No custom lifters — only use readily available, maintained libraries. VEX IR handles lifting and analysis. LLVM MC handles code emission (binary patching). LLVM is integrated minimally: only the MC layer and X86 target, built with `-DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS=""`. No Clang, no optimizer pipeline, no linker. Resulting binary size ~30-80MB, acceptable for a reversing suite.

## Build

```bash
cmake --preset x64-debug   # or x64-release
cmake --build out/build/x64-debug
```

Requires MSVC + Ninja. Boost must be at `C:/boost/x64/{debug,release}` or overridden via `-DBOOST_INSTALL=...`.

## Known Issues

1. ~~**Pass execution order broken**~~ — **FIXED:** Topological sort via `Dependencies()` declarations in `PassManager::RunAll()`.
2. **Duplicate type definitions** in `Win32InternalTypes.hpp` — lines 343-667 duplicate lines 7-331 outside the include guard and namespace.
3. **Thread safety** in `PassManager::RunAllAsync` — detached thread + `std::promise` lifetime risk.
4. **`const_cast` UB** in `AnalysisManager(const unique_ptr<AnalysisContext>&)` — moves from a const reference.
5. **O(n) function lookup** — `Module::GetFunctionForAddress` is a linear scan.
6. ~~**O(n) trace PC lookup**~~ — **FIXED:** `unordered_map` index in `SimpleTraceReader`.
7. ~~**FileLoader Win32-only**~~ — **FIXED:** `MemoryMapper` interface with `Win32MemoryMapper` / `PosixMemoryMapper`.
8. **Hardcoded CLI path** — `main.cpp:11` has a local absolute path.
9. **WHOLEARCHIVE CMake** — the per-pass `/WHOLEARCHIVE` logic uses incomplete object paths.
10. ~~**No test infrastructure**~~ — Google Test framework present, 31 tests passing.

### Project Database & Interactive UI (Phase 6)

ReWizard targets an IDA Pro-like interactive analysis experience. The architecture separates the **analysis database** (model) from the **UI** (view), enabling headless/scripting use alongside the GUI.

**Analysis Database** — persistent storage of all analysis results and user annotations:
- Functions (boundaries, types, calling conventions), BasicBlocks (CFG edges), Instructions (decoded bytes, operands)
- Cross-references (xrefs): call refs, data refs, jump refs — bidirectional
- Symbol annotations: renames, comments, type information
- Deobfuscation results: simplified IR, recovered control flow
- Hybrid trace data: execution records, resolved indirect targets
- Incremental: re-analysis only updates changed portions, doesn't rebuild from scratch
- Format: SQLite or custom binary format, loadable without re-running the full pipeline

**Interactive UI** — IDA Pro-inspired multi-panel layout:
- Disassembly view: instructions with symbol resolution, xref highlights, inline comments
- Graph view: Boost.Graph-derived CFG visualization with interactive navigation
- Hex view: raw bytes with decoded instruction overlay
- Function list: searchable/filterable function table with signature preview
- Cross-reference panel: jump-to-definition, jump-to-xref, call graph navigation
- Type viewer: struct/enum/typedef definitions with member layout
- Console/scripting: command-driven re-analysis, pass control, scripting API

## Project Goals

| Goal                        | Description |
|-----------------------------|-------------|
| **Static Analysis**         | Recursive descent disassembly, data flow, abstract interpretation |
| **Hybrid Analysis**         | Bochs full-system emulation with snapshot support; Unicorn optional micro-execution |
| **Deobfuscation**           | VEX IR-based transformations: constant folding, dead code elimination, CFF flattening recovery |
| **Cross-Platform**          | Windows first, Linux second, macOS if possible |
| **Multi-Architecture**      | VEX IR enables architecture-independent analysis passes (x86 now, ARM/MIPS future) |
| **Interactive Analysis**    | IDA Pro-like UI with analysis database, xrefs, type system, and graph visualization |