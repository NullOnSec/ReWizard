# ReWizard

Binary analysis framework for x86/x86_64 reverse engineering, built on a pass-based architecture. Uses LLVM IR for deobfuscation and code emission, Zydis for disassembly, and targets Bochs for full-system hybrid tracing.

## Features

- **Pass-based analysis pipeline** — passes auto-register via `PassRegistrar<T>`, executed in dependency order (topological sort)
- **Static analysis** — recursive descent disassembly, data flow analysis, abstract interpretation, opaque predicate detection
- **LLVM IR lifting** — manual Zydis+IRBuilder lifter lifts x86 basic blocks to LLVM IR (MOV, ADD, SUB, XOR, NOP, RET handled; more in progress)
- **LLVM optimization** — constant folding, dead code elimination, CFG simplification via LLVM's New Pass Manager (O2 pipeline)
- **Hybrid analysis** — trace-driven verification of static analysis results (trace file replay; Bochs full-system emulation planned)
- **Cross-platform** — Windows first, Linux/macOS support via platform abstraction layer

## Architecture

```
Binary → FileLoader (LIEF) → AnalysisContext → PassManager
                                    │
                          ┌─────────┴──────────┐
                          │   Analysis Passes    │
                          ├─────────────────────┤
                          │ ImportAnalysisPass   │
                          │ StaticControlFlow... │
                          │ DataFlowAnalysisPass │
                          │ AbstractInterpret... │
                          │ OpaquePredicatePass  │
                          │ IRLiftingPass        │
                          │ ConstantFoldingPass  │
                          │ HybridAnalysisPass   │
                          └─────────────────────┘
                                    │
                              Module (Functions → BasicBlocks → CFG)
                                    │
                              Lifter (Zydis → LLVM IR)
                                    │
                              LLVMOptimizer (SCCP, DCE, GVN, InstCombine)
```

## Passes

| Pass | Dependencies | Description |
|---|---|---|
| ImportAnalysisPass | (none) | PE import/export table parsing |
| StaticControlFlowRebuilder | ImportAnalysisPass | Recursive descent disassembly, CFG construction |
| DataFlowAnalysisPass | StaticControlFlowRebuilder | Register value tracking, indirect call resolution |
| AbstractInterpretationPass | StaticControlFlowRebuilder | Interval/domain analysis, opaque predicate detection |
| OpaquePredicatePass | AbstractInterpretationPass | Dead branch removal, CFG repair |
| IRLiftingPass | StaticControlFlowRebuilder | Lifts basic blocks to LLVM IR |
| ConstantFoldingPass | IRLiftingPass | LLVM O2 optimization on lifted IR |
| HybridAnalysisPass | StaticControlFlowRebuilder, AbstractInterpretationPass, OpaquePredicatePass | Trace-driven verification and re-analysis |

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| Zydis | v4.1.0 | x86/x86_64 instruction decode & format |
| LIEF | extended_build_patch | PE/ELF/MachO binary parsing |
| LLVM | 19.1.0 (X86 target only) | IR analysis, optimization, code emission |
| Boost | 1.85.0 | Graph (adjacency_list, GraphViz output) |
| spdlog | (via LIEF) | Logging |
| GoogleTest | v1.14.0 | Unit testing |
| Unicorn | v2.1.0 (optional) | CPU micro-execution |
| Bochs | (planned) | Full-system emulation |

## Building

### Prerequisites

- Visual Studio 2022 with C++20 support
- Ninja build system
- Boost 1.85.0 at `Z:/boost/boost_1_85_0` (or override with `-DBOOST_ROOT`)
- CMake 3.15+

### LLVM

One-time LLVM build (idempotent):

```powershell
.\scripts\build-llvm.ps1
```

This clones LLVM 19.1.0, configures a minimal Release build (X86 target only), and installs to `Z:/llvm-install`. ~30 minutes on first run.

### Configure and Build

Use the Developer Command Prompt or chain `vcvars64.bat`:

```powershell
# Configure
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --preset x64-release -DBOOST_ROOT=Z:/boost/boost_1_85_0"

# Build
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-release"
```

Debug builds are blocked by LLVM CRT mismatch (`/MDd` vs `/MD`). Use the Release preset.

### Running Tests

```powershell
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-release --target ReWizardTests"
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && ctest --test-dir out/build/x64-release"
```

## Project Structure

```
ReWizard/
├── ReWizardLib/           # Core library (analysis passes, IR, loader, hybrid)
│   ├── include/           # Public headers
│   └── source/            # Implementation
├── ReWizardCLI/           # CLI frontend
├── tests/                 # GoogleTest unit tests
│   └── fixtures/          # Test binaries (test_pe.exe)
├── scripts/               # Build scripts (build-llvm.ps1)
├── thirdparty/            # Vendored deps (Zydis, LIEF, Unicorn)
├── docs/                  # Architecture docs (PROJECT.md, WORK_PLAN.md)
└── NEXT_STEPS.md          # Current bug analysis and development roadmap
```

## Known Issues

- **ConstantFoldingPass crash** — per-function LLVM modules lack DataLayout/TargetTriple, causing 0xc0000005 in LLVM O2 pipeline. Fix in progress (shared module architecture). See `NEXT_STEPS.md`.
- **Debug build CRT mismatch** — LLVM built with `/MD` (Release CRT), project Debug uses `/MDd`. Use Release preset.
- **Duplicate type definitions** in `Win32InternalTypes.hpp` (lines 343-667 dup lines 7-331).
- **Thread safety** in `PassManager::RunAllAsync`.
- **Hardcoded CLI path** in `main.cpp`.

## Documentation

- `docs/PROJECT.md` — full architecture description, pass table, dependency list, project goals
- `docs/WORK_PLAN.md` — development roadmap and task breakdown
- `NEXT_STEPS.md` — current bug analysis, proposed fixes, and next-step priorities
- `AGENTS.md` — build commands, coding conventions, and contributor instructions

## License

Private project. All rights reserved.