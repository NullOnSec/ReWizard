# Next Session Plan

Updated: 2026-05-15

## What's Been Done

**Phase 0 — Stabilization (COMPLETE):**
- Fixed Win32InternalTypes.hpp, AnalysisManager UB, PassManager threading, CMake WHOLEARCHIVE, CLI arguments, tests

**Phase 1 — Import/Export Analysis (COMPLETE):**
- SymbolTable, ImportAnalysisPass (PE), symbol annotations in disassembly, PE relocation fixups, tests

**Phase 2 — Improved Static Analysis (COMPLETE):**
- ✅ 2.1 Recursive Descent Disassembly — entrypoint + exports + IAT targets, call following, linear sweep fallback
- ✅ 2.2 Data Flow Analysis — register tracking (mov/lea/xor), indirect call/jump resolution via known register values
- ✅ 2.3 Abstract Interpretation — interval/domain analysis, opaque predicate detection (test/cmp + jz/jnz/js/jns patterns)
- ✅ 2.4 Output Generation — AnalysisResult with JSON/DOT/text export, CLI --output/--format flags

**Phase 3 — Hybrid Analysis with PANDAS (COMPLETE):**
- ✅ 3.1 PANDAS Integration Layer — `Hybrid/TraceRecord.h`, `ITraceReader.h`, `SimpleTraceReader` (JSON trace format)
- ✅ 3.3 HybridAnalysisPass — consumes trace data, resolves indirect call/jump targets, clears disproven opaque predicates, marks functions hybrid-verified
- ✅ 3.4 Hybrid/Static Iteration — `StaticControlFlowRebuilder::ReAnalyzeFrom()` called from HybridAnalysisPass on resolved targets; new functions discovered and tested
- ⏳ 3.2 Trace Recording Workflow — document PANDAS replay creation (Windows VM, NT loader, begin_record/end_record)
- ⏳ 3.5 Tests with real PANDAS recording fixture

**Phase 4 — Deobfuscation (IN PROGRESS):**
- ✅ 4.1 OpaquePredicatePass — removes dead successors from always-true/always-false branches, rebuilds CFG
- ⏳ 4.2 DeobfuscationFlattenPass — pattern-match control-flow flattening dispatchers
- ⏳ 4.3 DeadCodeEliminationPass — remove unreachable basic blocks
- ⏳ 4.4 ConstantFoldingPass — evaluate constant expressions

**Phase 5 — Extended Features:**
- ⏳ Not started

## Recommended Next Steps (in priority order)

### 1. Phase 4.2 — DeobfuscationFlattenPass
- Pattern-match control-flow flattening dispatcher structures (switch-based state machines)
- Reconstruct original control flow
- Test against a flattened binary fixture

### 2. Phase 4.3 — DeadCodeEliminationPass
- Remove unreachable basic blocks after OpaquePredicatePass has removed edges
- Clean up functions with no reachable BBs

### 3. Phase 4.4 — ConstantFoldingPass
- Evaluate constant expressions using AbstractInterpretationPass interval results
- Simplify arithmetic identities (e.g., `xor rax, rax` → 0)

### 4. Phase 3.2 — Trace Recording Workflow Documentation
- Document how to create PANDAS replays in a Windows VM
- Export trace to the JSON format `SimpleTraceReader` expects

## Files Recently Modified

- `ReWizardLib/include/ReWizard/Analysis/Passes/OpaquePredicatePass.h` (new)
- `ReWizardLib/source/Analysis/Passes/OpaquePredicatePass.cpp` (new)
- `ReWizardLib/include/ReWizard/Analysis/Units/BasicBlock.h` (added RemoveSuccessor)
- `ReWizardLib/include/ReWizard/Analysis/Units/Function.h` (added opaquePredicateResults_, hybridVerified_)
- `ReWizardLib/source/Analysis/Passes/StaticControlFlowRebuilder.cpp` (added ReAnalyzeFrom)
- `tests/test_opaque_predicate.cpp` (new)
- `tests/test_hybrid_analysis.cpp` (added ResolvedTargetTriggersReAnalysis)

## Build Reminders

- Always use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && <command>"`
- Boost is at `Z:/boost/boost_1_85_0`
- Commit small, focused changes only
- Test before every commit
