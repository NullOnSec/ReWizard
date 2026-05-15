#include <ReWizard/Analysis/Passes/OpaquePredicatePass.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>

#include <spdlog/spdlog.h>

#define UNUSED(x) (void)x

namespace ReWizard {

    bool OpaquePredicatePass::PreRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool OpaquePredicatePass::PostRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool OpaquePredicatePass::Run(AnalysisContext* context) {
        if (!context)
            return false;

        auto module = context->GetModule();
        if (!module)
            return false;

        for (const auto& fn : module->GetFunctions()) {
            if (!fn->HasOpaquePredicates())
                continue;

            auto& results = fn->GetOpaquePredicateResults();
            bool modified = false;

            for (const auto& [addr, alwaysTaken] : results) {
                BasicBlock* bb = nullptr;
                for (const auto& bbPtr : fn->GetBasicBlocks()) {
                    if (addr >= bbPtr->GetStart() && addr < bbPtr->GetEnd()) {
                        bb = bbPtr.get();
                        break;
                    }
                }

                if (!bb) {
                    spdlog::warn("OpaquePredicatePass: could not find BB for opaque predicate at 0x{:x}", addr);
                    continue;
                }

                auto& succs = bb->GetSuccessors();
                if (succs.size() < 2) {
                    spdlog::debug("OpaquePredicatePass: BB at 0x{:x} has <2 successors, skipping", bb->GetStart());
                    continue;
                }

                // For conditional branches, HandleBranch adds:
                //   succs[0] = branch target (from immediate operand)
                //   succs[1] = fallthrough (pc + insn length)
                uintptr_t branchTarget = succs[0];
                uintptr_t fallthrough = succs[1];

                if (alwaysTaken) {
                    bb->RemoveSuccessor(fallthrough);
                    spdlog::info("OpaquePredicatePass: removing dead fallthrough 0x{:x} from BB 0x{:x} (always taken)",
                                 fallthrough, bb->GetStart());
                } else {
                    bb->RemoveSuccessor(branchTarget);
                    spdlog::info("OpaquePredicatePass: removing dead branch target 0x{:x} from BB 0x{:x} (always not taken)",
                                 branchTarget, bb->GetStart());
                }
                modified = true;
            }

            if (modified) {
                fn->BuildCFG();
                spdlog::info("OpaquePredicatePass: rebuilt CFG for {}", fn->GetName());
            }
        }

        return true;
    }

}
