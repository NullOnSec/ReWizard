#ifndef ANALYSIS_PASS_MANAGER_H
#define ANALYSIS_PASS_MANAGER_H

#include <ReWizard/Analysis/PassProvider.h>

namespace ReWizard {

    class AnalysisPassManager {
    public:
        AnalysisPassManager() = default;

        void AddPass(std::unique_ptr<BaseAnalysisPass>& p) {
            PassProvider::AddPass(p);
        }

        bool RunAll(AnalysisContext* ctx);

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
