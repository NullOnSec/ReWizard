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

Deobfuscation passes (constant folding, dead code elimination, CFF flattening recovery) operate on **VEX IR** — Valgrind's intermediate representation, maintained by angr as `libvex`. Operating on IR instead of raw x86 instructions:

- Makes transformations architecture-independent (same pass works on x86, ARM, MIPS)
- Provides explicit side-effects (register writes, memory stores, condition flags)
- Enables sound constant propagation and dead code elimination on a simple RISC-like representation
- Avoids the need for custom lifters — angr's `libvex` supports x86, AMD64, ARM, ARM64, MIPS, PPC

VEX IR is lifted from raw bytes via `IRSB` (IR Super Block) — each basic block becomes a sequence of typed statements (WrTmp, Put, Store, Exit) operating on temporaries.

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
| VEX IR   | angr/pyvex (C core)  | IR lifting for deobfuscation passes      |
| Boost    | 1.85.0                | Graph (adjacency_list, GraphViz output)   |
| spdlog   | (via LIEF)            | Logging                                  |

**Dependency policy:** No custom lifters — only use readily available, maintained libraries. VEX IR is the best fit: maintained by angr, C library compiles with MSVC, BSD-2-Clause license, supports multiple architectures. LLVM IR was considered but rejected as too large/heavyweight for our needs.

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

## Project Goals

| Goal                        | Description |
|-----------------------------|-------------|
| **Static Analysis**         | Recursive descent disassembly, data flow, abstract interpretation |
| **Hybrid Analysis**         | Bochs full-system emulation with snapshot support; Unicorn optional micro-execution |
| **Deobfuscation**           | VEX IR-based transformations: constant folding, dead code elimination, CFF flattening recovery |
| **Cross-Platform**          | Windows first, Linux second, macOS if possible |
| **Multi-Architecture**      | VEX IR enables architecture-independent analysis passes (x86 now, ARM/MIPS future) |