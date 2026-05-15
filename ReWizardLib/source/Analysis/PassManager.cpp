#include <ReWizard/Analysis/PassManager.h>

#include <spdlog/spdlog.h>

namespace ReWizard {

    bool AnalysisPassManager::RunAll(AnalysisContext* ctx) {
        bool ok = false;
        for (auto& [_, pass] : GetAllPasses()) {
            if (!pass) continue;

            spdlog::debug("Running {} ...", pass->Name());

            if (!(ok = pass->PreRun(ctx)))
                return false;

            if (!(ok = pass->Run(ctx)))
                return false;

            if (!(ok = pass->PostRun(ctx)))
                return false;
        }
        return ok;
    }

}
