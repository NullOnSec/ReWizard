#ifndef ANALYSIS_PASS_MANAGER_H
#define ANALYSIS_PASS_MANAGER_H

#include <ReWizard/Analysis/Passes/Passes.hpp>


#include <array>
#include <cstdint>
#include <thread>
#include <queue>
#include <mutex>
#include <future>

namespace ReWizard {

    class AnalysisPassManager {
    public:
        AnalysisPassManager() {
            m_passes[0] = (std::make_unique<StaticGenericPass>());
        }

        void RunAllAsync(AnalysisContext* ctx);

    private:
        std::array<std::unique_ptr<BaseAnalysisPass>, PassCount> m_passes;
    };

}

#endif
