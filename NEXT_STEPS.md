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

**Phase 3 — Hybrid Analysis with PANDAS (IN PROGRESS):**
- ✅ 3.1 PANDAS Integration Layer — `Hybrid/TraceRecord.h`, `ITraceReader.h`, `SimpleTraceReader` (JSON trace format)
- ✅ 3.3 HybridAnalysisPass — consumes trace data, resolves indirect call/jump targets, clears disproven opaque predicates, marks functions hybrid-verified
- ⏳ 3.2 Trace Recording Workflow — document PANDAS replay creation (Windows VM, NT loader, begin_record/end_record)
- ⏳ 3.4 Hybrid/Static Iteration — re-run StaticControlFlowRebuilder after hybrid resolves targets; pass dependency tracking
- ⏳ 3.5 Tests with real PANDAS recording fixture

**Phase 4 — Deobfuscation:**
- ⏳ Not started

**Phase 5 — Extended Features:**
- ⏳ Not started

## Recommended Next Steps (in priority order)

### 1. Phase 3.2 — Trace Recording Workflow Documentation
- Document how to create PANDAS replays in a Windows VM
- Boot Windows with PANDAS, use NT loader introspection to launch targets
- Record with `begin_record` / `end_record`
- Export trace to the JSON format `SimpleTraceReader` expects

### 2. Phase 3.4 — Hybrid/Static Iteration
- After HybridAnalysisPass resolves targets, trigger re-analysis of newly reachable functions
- Add pass dependency tracking to PassManager (hybrid must run after static)
- Consider adding an `AnalysisManager::ReRunPass(name)` mechanism

### 3. Phase 4.1 — OpaquePredicatePass
- Use AbstractInterpretationPass results to rewrite CFG
- Remove dead basic blocks from always-true/always-false branches
- Merge basic blocks where possible

### 4. Phase 4.2+ — Deobfuscation Passes
- DeobfuscationFlattenPass: pattern-match control-flow flattening dispatchers
- DeadCodeEliminationPass: remove unreachable basic blocks
- ConstantFoldingPass: evaluate constant expressions

## Files Recently Modified

- `ReWizardLib/include/ReWizard/Hybrid/TraceRecord.h` (new)
- `ReWizardLib/include/ReWizard/Hybrid/ITraceReader.h` (new)
- `ReWizardLib/include/ReWizard/Hybrid/SimpleTraceReader.h` (new)
- `ReWizardLib/source/Hybrid/SimpleTraceReader.cpp` (new)
- `ReWizardLib/include/ReWizard/Analysis/Passes/HybridAnalysisPass.h` (new)
- `ReWizardLib/source/Analysis/Passes/HybridAnalysisPass.cpp` (new)
- `ReWizardLib/source/Analysis/Passes/AbstractInterpretationPass.cpp` (bugfixes)
- `ReWizardCLI/main.cpp` (--trace flag)
- `tests/test_hybrid_analysis.cpp` (new)

## Build Reminders

- Always use `cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && <command>"`
- Boost is at `Z:/boost/boost_1_85_0`
- Commit small, focused changes only
- Test before every commit
