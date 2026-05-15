# Next Session Plan

Updated: 2026-05-15

## P0 BUG: ConstantFoldingPass Crash (0xc0000005 / Access Violation)

### Status: FIXED

### Symptoms

Running the full pipeline (IRLiftingPass -> ConstantFoldingPass) on `test_pe.exe` crashed
with `SEH exception 0xc0000005` inside `ConstantFoldingPass::Run()`, immediately when
`LLVMOptimizer::Run(llvmModule, O2)` was called.

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
   crash with 0xc0000005 when run on modules containing thousands of tiny orphan
   functions (no callers, no entry point).

### Pass Bisection Results

Systematic testing with empty vs single-pass FPM on the full test binary:

| Pass | Result |
|------|--------|
| Empty FPM | PASS |
| `llvm::DCEPass` | PASS |
| `llvm::SCCPPass` | PASS |
| `llvm::SimplifyCFGPass` | **CRASH (0xc0000005)** |
| `llvm::InstCombinePass` | **CRASH (0xc0000005)** |

**Safe pipeline:** `SCCPPass -> DCEPass` (constant propagation + dead code elimination)

### Fix Applied

1. **`Lifter(bool is64Bit)`** — sets DataLayout and TargetTriple in constructor
2. **`IRLiftingPass`** — uses single `std::unique_ptr<Lifter> lifter_` instead of vector
3. **`ConstantFoldingPass`** — finds module once, runs `LLVMOptimizer::Run()` once
4. **`LLVMOptimizer`** — replaced `buildPerModuleDefaultPipeline(O2)` with explicit
   `FunctionPassManager` containing `SCCPPass` + `DCEPass`

### Test Expectations After Fix

- `CFGRecoveryTest.BasicBlocksExistInFunctions` passes without access violation
- `ConstantFoldingPass` runs in <1s (single pass on one module)
- All 35 tests pass (2 skipped)

---

## Architecture Decisions Updated

### Hybrid Engine: Bochs Primary, Unicorn Optional

- **PANDAS is OUT** — Linux-only host, requires QEMU build, cannot run on Windows
- **Bochs is IN as primary backend** — full-system emulation, no API stubs needed, works on Windows/Linux/macOS
- **Unicorn is IN as optional accelerator** — for simple micro-execution (arithmetic predicates), with syscall service layer for common NT calls. Never the foundation.
- **No Python scripts** — everything embedded in C++. No "record on Linux / analyze on Windows" workflow.
- **Bochs snapshot strategy** — boot once, save CPU+memory state, restore for each analysis session

### IR: LLVM IR (manual lifter for now, remill planned)

- **Single IR, single toolchain** — LLVM IR for everything: lifting, analysis, optimization, code emission
- **Manual Zydis+IRBuilder lifter** currently lifts MOV, ADD, SUB, XOR, NOP, RET to LLVM IR
- **remill** (Trail of Bits) planned for comprehensive x86 semantics — blocked on build integration
- **LLVM optimizer passes** provide constant propagation (SCCP) and dead code elimination (DCE)
- **LLVM X86 backend** planned for binary patching (code emission)

### Bochs Rationale

The binspektor prototype needed **20 hooks (13 unique implementations) just for hello-world**. Real binaries hit hundreds of APIs. The syscall service layer (~25-30 NT handlers) reduces this but is still a maintenance trap. Bochs provides **correctness by default** with full OS emulation. Snapshot mitigates the boot-time penalty.

## What's Been Done

**Phase 0 — Stabilization (COMPLETE):**
- Fixed Win32InternalTypes.hpp, AnalysisManager UB, PassManager threading, CMake WHOLEARCHIVE, CLI arguments, tests

**Phase 1 — Import/Export Analysis (COMPLETE):**
- SymbolTable, ImportAnalysisPass (PE), symbol annotations in disassembly, PE relocation fixups, tests

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
- ConstantFoldingPass runs safe SCCP+DCE pipeline
- **FIXED**: Crash resolved via pass bisection and safe pipeline

**Phase 6 — UI/Database:** Not started

## Target Priority

**x64 PE > x86 PE > Linux ELF > Mach-O**

All test fixtures, primary development, and optimization effort target x64 PE first. x86 PE is supported secondarily. Linux ELF and Mach-O are tertiary/last priorities and are explicitly deferred until x64/x86 PE are fully functional.

## Recommended Next Steps (in priority order)

### 1. Expand Lifter Instruction Coverage

Add: PUSH, POP, CALL, JMP, CMP, conditional branches (JZ, JNZ, JG, JL, etc.), memory load/store (MOV r/m, LEA), TEST, AND, OR, SHL, SHR, NEG, NOT, MUL, DIV. Handle flags via a separate flags register in the alloca array.

### 2. Investigate SimplifyCFG/InstCombine Crash

The crash in `SimplifyCFGPass` and `InstCombinePass` suggests our lifted IR contains
an invalid construct that these passes trip over. Possible causes:
- `ret void` in functions with no callers (orphan functions)
- `alloca` of `i64[16]` array with GEP indices that confuse SROA/SimplifyCFG
- Missing `noundef` or other attributes expected by the New Pass Manager

Debug approach: dump the LLVM IR of the module before optimization, then use
`opt -passes=simplifycfg` on the IR file to see if it crashes standalone.
If so, bisect which function causes the crash by deleting functions until it passes.

### 3. Selective Lifting Scorer

Design a heuristic scorer that marks functions as "needs IR lifting" based on:
- Opaque predicate density (from AbstractInterpretationPass)
- Indirect call/jump count
- Code entropy (obfuscation indicator)
- Function size vs. complexity ratio

### 4. Bochs Integration

Design `IEmulator` interface, `BochsExecutor` with snapshot management, instrumentation hooks.

### 5. remill Integration (long-term)

Replace manual lifter with remill for comprehensive x86/x86_64 semantics. Requires remill build integration.

## Key Files

- `ReWizardLib/source/IR/Lifter.cpp` — Manual lifter (Zydis + IRBuilder), shared module
- `ReWizardLib/include/ReWizard/IR/Lifter.h` — Lifter interface
- `ReWizardLib/source/IR/IRBlock.cpp` — IRBlock wrapper (holds raw `llvm::BasicBlock*`)
- `ReWizardLib/source/IR/LLVMOptimizer.cpp` — LLVM New Pass Manager wrapper (safe SCCP+DCE pipeline)
- `ReWizardLib/source/Analysis/Passes/IRLiftingPass.cpp` — Creates single shared Lifter
- `ReWizardLib/include/ReWizard/Analysis/Passes/IRLiftingPass.hpp` — Pass header
- `ReWizardLib/source/Analysis/Passes/ConstantFoldingPass.cpp` — Runs optimization once on shared module
- `ReWizardLib/source/Analysis/PassProvider.cpp` — Pass auto-registration
- `ReWizardLib/source/Analysis/PassManager.cpp` — Topological sort and execution
- `scripts/build-llvm.ps1` — Idempotent LLVM 19.1.0 build script

## Build Reminders

- Use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && <command>"` (x64, not x86)
- Release preset required when LLVM enabled (Debug CRT mismatch)
- Boost at `Z:/boost/boost_1_85_0`, override with `-DBOOST_ROOT=Z:/boost/boost_1_85_0`
- LLVM at `Z:/llvm-install/` (full C++ build), fallback `Z:/llvm/` (C API only)
