# Next Session Plan

Updated: 2026-05-15

## What's Been Done

**Phase 0 — Stabilization (COMPLETE):**
- Fixed Win32InternalTypes.hpp, AnalysisManager UB, PassManager threading, CMake WHOLEARCHIVE, CLI arguments, tests

**Phase 1 — Import/Export Analysis (COMPLETE):**
- SymbolTable, ImportAnalysisPass (PE), symbol annotations in disassembly, PE relocation fixups, tests

**Phase 2 — Improved Static Analysis (IN PROGRESS):**
- ✅ 2.1 Recursive Descent Disassembly — entrypoint + exports + IAT targets, call following, linear sweep fallback
- ✅ 2.4 Output Generation — AnalysisResult with JSON/DOT/text export, CLI --output/--format flags
- ⏳ 2.2 Data Flow Analysis — NOT STARTED
- ⏳ 2.3 Abstract Interpretation — NOT STARTED

**Phase 3 — Hybrid Analysis with PANDAS:**
- ⏳ Not started

**Phase 4 — Deobfuscation:**
- ⏳ Not started

**Phase 5 — Extended Features:**
- ⏳ Not started

## Recommended Next Steps (in priority order)

### 1. Phase 2.2 — DataFlowAnalysisPass
- Track register definitions and uses within basic blocks
- Maintain a map of register -> value/origin for each BB
- Resolve indirect calls through register tracking (e.g., `call rax` where `rax` was loaded from a known address)
- Mark resolved indirect calls with their targets when possible
- Register as GenericPass, auto-register via PassRegistrar

### 2. Phase 2.3 — AbstractInterpretationPass  
- Implement interval/domain analysis for register values
- Replace the current `>30% stack ops` heuristic for marking functions for hybrid analysis
- Detect opaque predicates (always-true / always-false branches)
- Update StaticControlFlowRebuilder to consume abstract interpretation results

### 3. Phase 3.1 — PANDAS Integration Layer
- Add PANDAS as a git submodule or FetchContent dependency
- Create `Hybrid/PANDASRunner.h|.cpp` wrapper for replay recordings
- Document the trace recording workflow

### 4. Phase 3.3 — HybridAnalysisPass
- Consume PANDAS trace data
- Resolve indirect calls/jumps from runtime observations
- Mark functions as "hybrid-verified"

## Files Recently Modified

- `ReWizardLib/source/Analysis/Passes/StaticControlFlowRebuilder.cpp` (recursive descent)
- `ReWizardLib/source/Analysis/AnalysisResult.cpp` (output serialization)
- `ReWizardCLI/main.cpp` (CLI flags)
- `tests/fixtures/test_hello.exe` (new test fixture)

## Build Reminders

- Always use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && <command>"`
- Boost is at `Z:/boost/boost_1_85_0`
- Commit small, focused changes only
- Test before every commit
