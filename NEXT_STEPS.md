# Next Session Plan

Updated: 2026-05-16

## P0 BUG: ConstantFoldingPass Crash (0xc0000005 / Access Violation)

### Status: FIXED

### Root Cause Analysis

**Three compounding problems were identified and fixed:**

1. **Per-function `llvm::Module` with no DataLayout or TargetTriple.**
   Fixed by adding `Lifter(bool is64Bit)` constructor that sets correct DataLayout
   and TargetTriple based on binary architecture.

2. **ConstantFoldingPass ran O2 once per basic block per module.**
   Fixed by changing `IRLiftingPass` to use a single shared `Lifter` (and thus single
   shared `llvm::Module`), and changing `ConstantFoldingPass` to find the module once
   and run optimization once.

3. **LLVM `buildPerModuleDefaultPipeline(O2)` crashed on orphan functions.**
   The full O2 pipeline includes `SimplifyCFGPass` and `InstCombinePass`, both of which
   crashed with 0xc0000005 when run on modules containing thousands of tiny orphan
   functions (no callers, no entry point) with `ExternalLinkage`.

### Pass Bisection Results

| Pass | Result |
|------|--------|
| Empty FPM | PASS |
| `llvm::DCEPass` | PASS |
| `llvm::SCCPPass` | PASS |
| `llvm::SimplifyCFGPass` | **CRASH (0xc0000005)** |
| `llvm::InstCombinePass` | **CRASH (0xc0000005)** |

### Fix Applied

1. **`Lifter(bool is64Bit)`** — sets DataLayout and TargetTriple in constructor
2. **`IRLiftingPass`** — uses single `std::unique_ptr<Lifter> lifter_` instead of vector
3. **`ConstantFoldingPass`** — finds module once, runs `LLVMOptimizer::Run()` once
4. **`LLVMOptimizer`** — replaced `buildPerModuleDefaultPipeline(O2)` with explicit pipeline
5. **`Lifter::LiftBasicBlock`** — changed `ExternalLinkage` to `InternalLinkage` so `GlobalDCEPass` can remove dead functions
6. **`LLVMOptimizer`** — added `verifyModule()` before and after optimization
7. **`LLVMOptimizer`** — enabled `SimplifyCFGPass`, `InstCombinePass`, `GlobalDCEPass` in pipeline

**Full pipeline: `SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE`**

All 37 tests pass (2 skipped).

---

## CFG Recovery Bugs Fixed

### Mega-Function Bug (HandleCall)
`HandleCall` was pushing direct call targets into local `work` queue instead of outer `functionWork` queue, causing callees to be recursively inlined into the caller. Fixed by threading `functionWork` through `AnalyzeFunction → HandleCall`.

### IAT Entry Point Bug
`CollectEntryPoints` was adding IAT slot addresses (data-section pointers) as function entry points, producing garbage functions. Removed the import-IAT loop.

### Executable-Section Guards
`IsExecutableAddress()` helper checks `IMAGE_SCN_MEM_EXECUTE` / `SHF_EXECINSTR` before creating functions or enqueuing targets.

### GetFunctionForAddress Off-By-One
Changed `address <= fn->GetEnd()` to `address < fn->GetEnd()` (exclusive end).

### JMP Tail-Call Detection
Unconditional JMPs to known function starts now record a call site instead of inlining. Unknown JMP targets are pushed to both `work` and `functionWork`. Prevents tail-call inlining for known functions while remaining conservative for undiscovered targets.

---

## Architecture Decisions Updated

### LLVM Optimization Pipeline
- **Full pipeline enabled**: `SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE`
- **InternalLinkage**: All lifted functions use `InternalLinkage` so `GlobalDCEPass` can remove orphans
- **verifyModule()**: IR validation before and after optimization catches malformed IR early

### Hybrid Engine: Bochs Primary, Unicorn Optional
- **PANDAS is OUT** — Linux-only host, requires QEMU build, cannot run on Windows
- **Bochs is IN as primary backend** — full-system emulation, no API stubs needed
- **Unicorn is IN as optional accelerator** — for simple micro-execution
- **No Python scripts** — everything embedded in C++

### IR: LLVM IR (manual lifter for now, remill planned)
- **Single IR, single toolchain** — LLVM IR for everything
- **Manual Zydis+IRBuilder lifter** currently lifts MOV, ADD, SUB, XOR, NOP, RET
- **remill** (Trail of Bits) planned for comprehensive x86 semantics

## What's Been Done

**Phase 0 — Stabilization (COMPLETE):**
- Fixed Win32InternalTypes.hpp, AnalysisManager UB, PassManager threading, CMake WHOLEARCHIVE, CLI arguments, tests

**Phase 1 — Import/Export Analysis (COMPLETE):**
- SymbolTable, ImportAnalysisPass (PE), symbol annotations, PE relocation fixups, tests

**Phase 2 — Improved Static Analysis (COMPLETE):**
- Recursive descent disassembly, data flow analysis, abstract interpretation, opaque predicate detection, output generation

**Phase 3 — Hybrid Analysis (IN PROGRESS):**
- Trace infrastructure, platform abstraction, HybridAnalysisPass, SimpleTraceReader O(1) lookup
- Pass dependency system — topological sort fixes critical bug where 4/6 passes silently no-opped
- Bochs integration pending

**Phase 4 — IR & Deobfuscation (IN PROGRESS):**
- LLVM 19.1.0 built from source, C++ libraries linked
- Manual Lifter (Zydis + IRBuilder) lifts MOV, ADD, SUB, XOR, NOP, RET
- IRLiftingPass auto-registered, lifts all basic blocks into single shared module
- ConstantFoldingPass runs full optimization pipeline: SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE
- **FIXED**: Crash resolved via `InternalLinkage` + `GlobalDCEPass` + `verifyModule()`
- **FIXED**: CFG recovery bugs (mega-function, IAT entry points, off-by-one, tail-call detection)

**Phase 6 — UI/Database:** Not started

## Target Priority

**x64 PE > x86 PE > Linux ELF > Mach-O**

## Recommended Next Steps (in priority order)

### 1. Expand Lifter Instruction Coverage

Add: PUSH, POP, CALL, JMP, CMP, conditional branches (JZ, JNZ, JG, JL, etc.), memory load/store (MOV r/m, LEA), TEST, AND, OR, SHL, SHR, NEG, NOT, MUL, DIV. Handle flags via a separate flags register in the alloca array.

### 2. Bochs Integration

Design `IEmulator` interface, `BochsExecutor` with snapshot management, instrumentation hooks.

### 3. remill Integration (long-term)

Replace manual lifter with remill for comprehensive x86/x86_64 semantics. Requires remill build integration.

### 4. Selective Lifting Scorer

Design a heuristic scorer that marks functions as "needs IR lifting" based on:
- Opaque predicate density (from AbstractInterpretationPass)
- Indirect call/jump count
- Code entropy (obfuscation indicator)
- Function size vs. complexity ratio

### 5. O(n) Function Lookup

Replace linear scan in `Module::GetFunctionForAddress` with an interval tree or sorted vector + binary search.

## Known Issues / Limitations

- **JMP tail calls to undiscovered functions**: If a JMP target hasn't been discovered as a function yet, it will be inlined into the current function. Only JMPs to already-known function starts are correctly treated as tail calls.
- **Debug builds blocked**: LLVM built with `/MD` (Release CRT), project Debug uses `/MDd`. Use `x64-release` preset only.
- **`sub_1400021b0`**: Found by `LinearSweepFallback` in merged section — not a real function but passes exec check.
- **Duplicate type definitions** in `Win32InternalTypes.hpp` — lines 343-667 duplicate lines 7-331.
- **Thread safety** in `PassManager::RunAllAsync` — detached thread + `std::promise` lifetime risk.
- **O(n) function lookup** — `Module::GetFunctionForAddress` is a linear scan.
- **Hardcoded CLI path** — `main.cpp:11` has a local absolute path.
- **WHOLEARCHIVE CMake** — the per-pass logic uses incomplete object paths.

## Key Files

- `ReWizardLib/source/IR/Lifter.cpp` — Manual lifter (Zydis + IRBuilder), shared module, `InternalLinkage`
- `ReWizardLib/source/IR/LLVMOptimizer.cpp` — Full pipeline: SCCP → DCE → SimplifyCFG → InstCombine → GlobalDCE, `verifyModule()`
- `ReWizardLib/source/IR/IRBlock.cpp` — IRBlock wrapper (holds raw `llvm::BasicBlock*`)
- `ReWizardLib/source/Analysis/Passes/IRLiftingPass.cpp` — Creates single shared Lifter
- `ReWizardLib/source/Analysis/Passes/ConstantFoldingPass.cpp` — Runs optimization once on shared module
- `ReWizardLib/source/Analysis/Passes/StaticControlFlowRebuilder.cpp` — CALL fix, IAT removal, exec-section guards, JMP tail-call detection
- `ReWizardLib/include/ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h` — Updated `HandleBranch` and `AnalyzeFunction` signatures
- `ReWizardLib/source/Analysis/Units/Module.cpp` — `GetFunctionForAddress` exclusive-end fix
- `tests/test_cfg.cpp` — NoMegaFunctions, FunctionsAreNotInlined regression tests
- `scripts/build-llvm.ps1` — Idempotent LLVM 19.1.0 build script

## Build Reminders

- Use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && <command>"` (x64, not x86)
- Release preset required when LLVM enabled (Debug CRT mismatch)
- Boost at `Z:/boost/boost_1_85_0`, override with `-DBOOST_ROOT=Z:/boost/boost_1_85_0`
- LLVM at `Z:/llvm-install/` (full C++ build), fallback `Z:/llvm/` (C API only)