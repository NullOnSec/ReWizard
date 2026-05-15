#include <ReWizard/IR/LLVMOptimizer.h>

#ifdef REWIZARD_LLVM_ENABLED
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Transforms/Scalar/SCCP.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/ADCE.h>
#include <llvm/Support/raw_ostream.h>
#endif

#include <spdlog/spdlog.h>

namespace ReWizard {

    bool LLVMOptimizer::Run(llvm::Module* module, Level level) {
#ifndef REWIZARD_LLVM_ENABLED
        (void)module;
        (void)level;
        spdlog::warn("LLVMOptimizer::Run: LLVM not enabled");
        return false;
#else
        if (!module) {
            spdlog::warn("LLVMOptimizer::Run: null module");
            return false;
        }

        spdlog::debug("LLVMOptimizer: running optimization level {}", static_cast<int>(level));

        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        llvm::PassBuilder PB;
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        llvm::ModulePassManager MPM;

        if (level != Level::O0) {
            // Build a minimal deobfuscation pipeline explicitly.
            // The full buildPerModuleDefaultPipeline(O2) crashes on modules
            // with thousands of tiny orphan functions (no callers, no entry point).
            //
            // Bisection results (crash = 0xc0000005):
            //   PASS: Empty FPM, DCEPass, SCCPPass
            //   FAIL: SimplifyCFGPass, InstCombinePass
            // Safe pipeline: SCCP -> DCE to fold constants and remove dead code.
            llvm::FunctionPassManager FPM;
            FPM.addPass(llvm::SCCPPass());
            FPM.addPass(llvm::DCEPass());
            MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
        }

        auto result = MPM.run(*module, MAM);
        (void)result;

        spdlog::debug("LLVMOptimizer: optimization complete");
        return true;
#endif
    }

    std::string LLVMOptimizer::DumpIR(llvm::Module* module) {
#ifndef REWIZARD_LLVM_ENABLED
        (void)module;
        return "; LLVM not enabled\n";
#else
        if (!module)
            return "; null module\n";

        std::string str;
        llvm::raw_string_ostream os(str);
        module->print(os, nullptr);
        return str;
#endif
    }

}
