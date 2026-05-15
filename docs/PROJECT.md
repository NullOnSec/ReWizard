# ReWizard

Binary analysis framework for x86/x86_64 reverse engineering, built on a pass-based architecture.

## Architecture

ReWizard follows a **pass-based analysis pipeline** (inspired by compiler IR passes). An `AnalysisManager` owns an `AnalysisContext` (holding the binary mapping, module, and disassembler) and an `AnalysisPassManager` that runs registered passes sequentially. Passes auto-register via `PassRegistrar<T>` with static initialization and are instantiated by `PassProvider::Init()`.

```
AnalysisManager
 ├── AnalysisContext
 │    ├── FileLoader      (LIEF parser + VirtualAlloc memory mapping)
 │    ├── Module          (owns Functions → BasicBlocks → CFG)
 │    ├── Disassembler    (Zydis v4.1.0, thread-safe via Proxy/Mutex)
 │    └── visited addrs   (std::set<uintptr_t>)
 └── AnalysisPassManager
      └── PassProvider → StaticControlFlowRebuilder (auto-registered)
```

### Data Model

- **Module** — top-level container; holds all `Function`s and the global instruction map (`std::map<uintptr_t, unique_ptr<ExtendedInstruction>>`).
- **Function** — boundaries (`start`/`end`/`lastInsnAddr`), `CallSite` set, `BasicBlock` collection, Boost.Graph CFG, opaque-predicate heuristic flag, trampoline flag, disassembly cache.
- **BasicBlock** — start/end addresses, successor/predecessor address lists, `Split()` for re-structuring, indirect-call/jump flags.
- **DecodedInstruction / ExtendedInstruction** — wraps Zydis decoded instruction + operands; `ExtendedInstruction` adds `IsIndirect` and `IndirectValue`.

### Current Passes

| Pass                    | Type          | Status |
|-------------------------|---------------|--------|
| StaticControlFlowRebuilder | GenericPass  | Working (linear sweep + call/branch/trampoline detection) |

### Third-Party Dependencies

| Library  | Version / Branch      | Purpose                                  |
|----------|-----------------------|------------------------------------------|
| Zydis    | v4.1.0                | x86/x86_64 instruction decode & format   |
| LIEF     | extended_build_patch  | PE/ELF/MachO binary parsing             |
| Unicorn  | v2.1.0 (x86 only)     | CPU emulation (linked but **unused**)   |
| Boost    | 1.85.0                | Graph (adjacency_list, GraphViz output)  |
| spdlog   | (via LIEF)            | Logging                                  |

## Build

```bash
cmake --preset x64-debug   # or x64-release
cmake --build out/build/x64-debug
```

Requires MSVC + Ninja. Boost must be at `C:/boost/x64/{debug,release}` or overridden via `-DBOOST_INSTALL=...`.

## Known Issues

1. **Duplicate type definitions** in `Win32InternalTypes.hpp` — lines 343-667 duplicate lines 7-331 outside the include guard and namespace, causing redefinition errors on non-MSVC toolchains.
2. **Thread safety** in `PassManager::RunAllAsync` — detached thread + `std::promise` lifetime risk; effectively synchronous but leaks thread resources.
3. **`const_cast` UB** in `AnalysisManager(const unique_ptr<AnalysisContext>&)` — moves from a const reference.
4. **O(n) function lookup** — `Module::GetFunctionForAddress` is a linear scan; needs an interval map or sorted structure for large binaries.
5. **No relocation fixups** — `FileLoader::Load()` maps sections at their virtual addresses but never applies relocations; absolute references are wrong if `VirtualAlloc` rebases.
6. **Dead Unicorn dependency** — linked and compiled but never used; intended for the unimplemented hybrid analysis pass.
7. **Hardcoded CLI path** — `main.cpp:11` has a local absolute path.
8. **WHOLEARCHIVE CMake** — the per-pass `/WHOLEARCHIVE` logic in `ReWizardLib/CMakeLists.txt` uses incomplete object paths.
9. **No test infrastructure** — zero tests exist currently.

## Project Goals

| Goal                        | Description |
|-----------------------------|-------------|
| **Static Analysis**         | Linear sweep + recursive descent disassembly (partially working) |
| **Hybrid Analysis**         | Dynamic trace via PANDAS replay — load ntoskrnl, use its loader to launch user-mode targets |
| **Abstract Interpretation** | Value analysis / interval analysis to resolve indirect calls and detect opaque predicates |
| **Deobfuscation**           | Leverage earlier analysis passes to deobfuscate opaque control flow and apply compiler optimizations |