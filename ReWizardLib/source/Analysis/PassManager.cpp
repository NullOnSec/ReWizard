#include <ReWizard/Analysis/PassManager.h>

#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace ReWizard {

    std::vector<BaseAnalysisPass*> AnalysisPassManager::TopologicalSort() {
        auto& allPasses = GetAllPasses();

        std::unordered_map<std::string, BaseAnalysisPass*> passByName;
        std::unordered_map<std::string, std::vector<std::string>> adj;
        std::unordered_map<std::string, int> inDegree;

        for (auto& [name, pass] : allPasses) {
            passByName[name] = pass.get();
            inDegree[name] = 0;
        }

        for (auto& [name, pass] : allPasses) {
            for (auto dep : pass->Dependencies()) {
                std::string depStr(dep);
                if (passByName.find(depStr) == passByName.end()) {
                    spdlog::warn("Pass '{}' declares dependency on '{}', but that pass is not registered", name, depStr);
                    continue;
                }
                adj[depStr].push_back(name);
                inDegree[name]++;
            }
        }

        std::queue<std::string> q;
        for (auto& [name, deg] : inDegree) {
            if (deg == 0) {
                q.push(name);
            }
        }

        std::vector<BaseAnalysisPass*> result;
        result.reserve(allPasses.size());

        while (!q.empty()) {
            auto name = q.front();
            q.pop();
            result.push_back(passByName[name]);

            for (auto& dependent : adj[name]) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    q.push(dependent);
                }
            }
        }

        if (result.size() != allPasses.size()) {
            spdlog::error("Pass dependency cycle detected! Only {}/{} passes can be ordered.", result.size(), allPasses.size());
        }

        return result;
    }

    bool AnalysisPassManager::RunAll(AnalysisContext* ctx) {
        auto passes = TopologicalSort();

        bool ok = false;
        for (auto* pass : passes) {
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
