#ifndef ANALYSIS_PASS_MANAGER_H
#define ANALYSIS_PASS_MANAGER_H

#include <ReWizard/Analysis/Passes/PassProvider.h>


#include <array>
#include <cstdint>
#include <thread>
#include <queue>
#include <mutex>
#include <future>

namespace ReWizard {

    class AnalysisPassManager {
    public:
        AnalysisPassManager() { }

        void AddPass(std::unique_ptr<BaseAnalysisPass>& p) {
            PassProvider::AddPass(p);
        }

        void RunAllAsync(AnalysisContext* ctx);
        std::future<bool> RunAsync(AnalysisContext* ctx, BaseAnalysisPass* pass);

        BaseAnalysisPass* GetPassByName(const std::string& name) {
            auto p = PassProvider::Get(name);
            if (!p.has_value())
                return nullptr;
            return p.value().second;
        }

        const PassMap& GetAllPasses() {
            return PassProvider::GetAll();
        }

    };

}

#endif
