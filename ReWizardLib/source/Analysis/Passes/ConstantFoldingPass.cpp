#include <ReWizard/Analysis/Passes/ConstantFoldingPass.hpp>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/IR/LLVMOptimizer.h>

#ifdef REWIZARD_LLVM_ENABLED
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#endif

#include <spdlog/spdlog.h>

namespace ReWizard {

    bool ConstantFoldingPass::PreRun(AnalysisContext* context) {
        (void)context;
        return true;
    }

    bool ConstantFoldingPass::PostRun(AnalysisContext* context) {
        (void)context;
        return true;
    }

    bool ConstantFoldingPass::Run(AnalysisContext* context) {
        if (!context)
            return false;

        auto module = context->GetModule();
        if (!module)
            return false;

        spdlog::info("ConstantFoldingPass: running LLVM optimizations on lifted IR");

        llvm::Module* llvmModule = nullptr;
        size_t totalBlocks = 0;
        size_t liftedBlocks = 0;

        for (auto& function : module->GetFunctions()) {
            if (!function)
                continue;

            for (auto& bb : function->GetBasicBlocks()) {
                if (!bb)
                    continue;

                totalBlocks++;

                auto* irBlock = bb->GetIRBlock();
                if (!irBlock || !irBlock->IsValid())
                    continue;

                liftedBlocks++;

                if (!llvmModule) {
                    auto* llvmBB = irBlock->GetLLVMBlock();
                    if (llvmBB) {
                        auto* llvmFunc = llvmBB->getParent();
                        if (llvmFunc) {
                            llvmModule = llvmFunc->getParent();
                        }
                    }
                }
            }
        }

        if (!llvmModule) {
            spdlog::info("ConstantFoldingPass: no lifted IR found ({}/{} blocks), skipping", liftedBlocks, totalBlocks);
            return true;
        }

        spdlog::info("ConstantFoldingPass: running LLVM optimization on module with {} lifted blocks", liftedBlocks);
        bool ok = LLVMOptimizer::Run(llvmModule, LLVMOptimizer::Level::O2, true);
        if (!ok) {
            spdlog::warn("ConstantFoldingPass: LLVM optimization failed");
        }

        return true;
    }

}
