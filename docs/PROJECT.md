# ReWizard

Binary analysis framework for x86/x86_64 reverse engineering, built on a pass-based architecture. Uses LLVM IR for deobfuscation and code emission, remill for binary lifting.

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

Deobfuscation uses **LLVM IR** as the single intermediate representation. Remill (Trail of Bits) lifts x86 binary bytes → LLVM IR. LLVM's optimization passes handle constant folding, dead code elimination, and CFG simplification. The X86 backend emits correct machine code for binary patching.

**One IR, one toolchain:**

```
Binary bytes ──(remill)──► LLVM IR ──(LLVM opts / custom passes)──► Optimized LLVM IR ──(X86 backend)──► Patched bytes
```

- **remill** lifts x86/x86_64 binary instructions to LLVM IR — makes all side-effects explicit (register writes, memory stores, condition flags)
- **LLVM optimizer passes** provide battle-tested constant propagation (SCCP), dead code elimination (DCE), CFG simplification, and global value numbering (GVN)
- **Custom ReWizard passes** operate on LLVM IR for CFF unflattening, opaque predicate simplification, and deobfuscation-specific transformations
- **LLVM X86 backend** emits correct machine code — handles x86 encoding complexity (prefixes, ModRM, VEX/EVEX, REX.W) so we don't have to

**We use existing lifters, not our own.** remill is maintained by Trail of Bits, used in production (McSema2), and supports x86, AMD64, AArch64.

**LLVM is integrated minimally.** Built with `-DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS=""`. No Clang, no linker, no frontend. ~2-3GB built, ~30-80MB linked binary size. Acceptable for a reversing suite.

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
| LLVM     | v19+ (X86 target only) | IR analysis, optimization passes, X86 code emission |
| remill   | (Trail of Bits)       | Binary lifter: x86/x86_64 bytes → LLVM IR |
| Unicorn  | v2.1.0 (x86 only)     | CPU micro-execution (optional accelerator) |
| Bochs    | (TBD)                 | Full-system emulation (primary backend)   |
| Boost    | 1.85.0                | Graph (adjacency_list, GraphViz output)   |
| spdlog   | (via LIEF)            | Logging                                  |

**Dependency policy:** No custom lifters or code emitters — only use readily available, maintained libraries. remill provides the lifter. LLVM provides the optimizer and code emitter. ~2-3GB built is acceptable for a full reversing suite.

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
10. ~~**No test infrastructure**~~ — Google Test framework present, 37 tests passing.
11. ~~**P0: ConstantFoldingPass crash (0xc0000005)**~~ — **FIXED.** Root cause was `SimplifyCFGPass` and `InstCombinePass` crashing on lifted IR with thousands of orphan functions. Bisection confirmed safe passes: `SCCPPass` + `DCEPass`. Unsafe passes: `SimplifyCFGPass`, `InstCombinePass`. Fix: (a) single shared `llvm::Module` with proper DataLayout/TargetTriple; (b) `ConstantFoldingPass` runs optimization once per module instead of per-BB; (c) safe explicit pipeline replaces `buildPerModuleDefaultPipeline(O2)`. All 35 tests pass.

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
| **Deobfuscation**           | LLVM IR-based transformations: constant folding, dead code elimination, CFF flattening recovery |
| **Binary Patching**         | remill lifts x86→LLVM IR, LLVM X86 backend emits optimized code, patch binary in-place |
| **Cross-Platform**          | Windows first (x64 PE primary, x86 PE secondary), Linux third (ELF), macOS last (Mach-O) |
| **Multi-Architecture**      | x64 PE is the primary target; x86 PE secondary. Linux ELF and Mach-O deferred. ARM future via LLVM + remill |
| **Interactive Analysis**    | IDA Pro-like UI with analysis database, xrefs, type system, and graph visualization |