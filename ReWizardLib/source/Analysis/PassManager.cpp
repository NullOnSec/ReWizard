#include <ReWizard/Analysis/PassManager.h>

#include <spdlog/spdlog.h>

namespace ReWizard {

    bool AnalysisPassManager::ExecAll(AnalysisContext* ctx) {
        if (!ctx) {
            return false;
        }

        for (auto& [_, pass] : GetAllPasses()) {
            if (!pass) {
                return false;
            }

            spdlog::debug("Running {} ...", pass->Name());

            if (!ExecPass(ctx, pass.get())) {
                spdlog::error("Error executing {}", pass->Name());
                return false;
            }

        }

        return true;
    }

    bool AnalysisPassManager::ExecPass(AnalysisContext* ctx, BaseAnalysisPass* pass) {
        if (!ctx || !pass) {
            return false;
        }

        if (!pass->PreRun(ctx)) {
            return false;
        }
        
        if (!pass->Run(ctx)) {
            return false;
        }

        if (!pass->PostRun(ctx)) {
            return false;
        }

        return true;
    }


}
