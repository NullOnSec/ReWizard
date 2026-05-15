# ReWizard

Binary analysis framework for x86/x86_64 reverse engineering, built on a pass-based architecture.

## Architecture

ReWizard follows a **pass-based analysis pipeline** (inspired by compiler IR passes). An `AnalysisManager` owns an `AnalysisContext` (holding the binary mapping, module, and disassembler) and an `AnalysisPassManager` that runs registered passes in dependency order (topological sort of `RunAfter()` declarations). Passes auto-register via `PassRegistrar<T>` with static initialization and are instantiated by `PassProvider::Init()`.

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

### Current Passes

| Pass                    | Type          | Status |
|-------------------------|---------------|--------|
| ImportAnalysisPass      | PEPass        | Working |
| StaticControlFlowRebuilder | GenericPass | Working (recursive descent + linear sweep fallback) |
| DataFlowAnalysisPass    | GenericPass   | Working |
| AbstractInterpretationPass | GenericPass | Working |
| OpaquePredicatePass     | GenericPass   | Working |
| HybridAnalysisPass      | GenericPass   | Working (consumes ITraceReader) |

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
| LIEF     | extended_build_patch  | PE/ELF/MachO binary parsing             |
| Unicorn  | v2.1.0 (x86 only)     | CPU micro-execution (optional accelerator) |
| Bochs    | (TBD)                 | Full-system emulation (primary backend)  |
| Boost    | 1.85.0                | Graph (adjacency_list, GraphViz output)  |
| spdlog   | (via LIEF)            | Logging                                  |

## Build

```bash
cmake --preset x64-debug   # or x64-release
cmake --build out/build/x64-debug
```

Requires MSVC + Ninja. Boost must be at `C:/boost/x64/{debug,release}` or overridden via `-DBOOST_INSTALL=...`.

## Known Issues

1. **Pass execution order broken** — `PassProvider` stores passes in `std::map` (alphabetical by name), causing `StaticControlFlowRebuilder` to run last. 4 out of 6 passes silently do nothing because they iterate empty function lists. **CRITICAL.**
2. **Duplicate type definitions** in `Win32InternalTypes.hpp` — lines 343-667 duplicate lines 7-331 outside the include guard and namespace, causing redefinition errors on non-MSVC toolchains.
3. **Thread safety** in `PassManager::RunAllAsync` — detached thread + `std::promise` lifetime risk; effectively synchronous but leaks thread resources.
4. **`const_cast` UB** in `AnalysisManager(const unique_ptr<AnalysisContext>&)` — moves from a const reference.
5. **O(n) function lookup** — `Module::GetFunctionForAddress` is a linear scan; needs an interval map or sorted structure for large binaries.
6. **O(n) trace PC lookup** — `SimpleTraceReader::GetRecordsForPC()` linearly scans all records per query.
7. **FileLoader Win32-only** — hard-coded `VirtualAlloc` / `VirtualFree` with no platform abstraction; blocks Linux/macOS compilation.
8. **Hardcoded CLI path** — `main.cpp:11` has a local absolute path.
9. **WHOLEARCHIVE CMake** — the per-pass `/WHOLEARCHIVE` logic in `ReWizardLib/CMakeLists.txt` uses incomplete object paths.
10. **No test infrastructure** — zero tests exist currently.

## Project Goals

| Goal                        | Description |
|-----------------------------|-------------|
| **Static Analysis**         | Recursive descent disassembly, data flow, abstract interpretation |
| **Hybrid Analysis**         | Bochs full-system emulation with snapshot support; Unicorn optional micro-execution |
| **Deobfuscation**           | Opaque predicate elimination, CFF flattening recovery, dead code elimination |
| **Cross-Platform**          | Windows first, Linux second, macOS if possible |
