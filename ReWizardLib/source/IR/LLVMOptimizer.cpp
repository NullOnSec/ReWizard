#include <ReWizard/IR/LLVMOptimizer.h>

#ifdef REWIZARD_LLVM_ENABLED
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Transforms/Scalar/SCCP.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/ADCE.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Support/raw_ostream.h>
#endif

#include <spdlog/spdlog.h>

namespace ReWizard {

    bool LLVMOptimizer::Run(llvm::Module* module, Level level, bool dumpBefore) {
#ifndef REWIZARD_LLVM_ENABLED
        (void)module;
        (void)level;
        (void)dumpBefore;
        spdlog::warn("LLVMOptimizer::Run: LLVM not enabled");
        return false;
#else
        if (!module) {
            spdlog::warn("LLVMOptimizer::Run: null module");
            return false;
        }

        spdlog::debug("LLVMOptimizer: running optimization level {}", static_cast<int>(level));

        std::string verifyErr;
        llvm::raw_string_ostream verifyErrOS(verifyErr);
        if (llvm::verifyModule(*module, &verifyErrOS)) {
            verifyErrOS.flush();
            spdlog::warn("LLVMOptimizer: module verification FAILED before optimization: {}", verifyErr);
        } else {
            spdlog::debug("LLVMOptimizer: module verification passed before optimization");
        }

        if (dumpBefore) {
            std::error_code ec;
            llvm::raw_fd_ostream out("rewizard_pre_opt.ll", ec);
            if (!ec) {
                module->print(out, nullptr);
                spdlog::info("LLVMOptimizer: dumped pre-optimization IR to rewizard_pre_opt.ll");
            } else {
                spdlog::warn("LLVMOptimizer: failed to dump pre-optimization IR: {}", ec.message());
            }
        }

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
            llvm::FunctionPassManager FPM;
            FPM.addPass(llvm::SCCPPass());
            FPM.addPass(llvm::DCEPass());
            FPM.addPass(llvm::SimplifyCFGPass());
            FPM.addPass(llvm::InstCombinePass());
            MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
            MPM.addPass(llvm::GlobalDCEPass());
        }

        auto result = MPM.run(*module, MAM);
        (void)result;

        verifyErr.clear();
        llvm::raw_string_ostream verifyErrOS2(verifyErr);
        if (llvm::verifyModule(*module, &verifyErrOS2)) {
            verifyErrOS2.flush();
            spdlog::warn("LLVMOptimizer: module verification FAILED after optimization: {}", verifyErr);
        } else {
            spdlog::debug("LLVMOptimizer: module verification passed after optimization");
        }

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
