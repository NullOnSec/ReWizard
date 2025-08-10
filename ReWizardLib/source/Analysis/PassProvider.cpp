#include <spdlog/spdlog.h>
#include <ReWizard/Analysis/PassProvider.h>

/*
    All the passes must be included in this TU to ensure
    that they are automatically registered
*/
#include <ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h>

namespace ReWizard {

    std::vector<PassRegistry::Factory>& PassRegistry::Factories() {
        static std::vector<Factory> factories;
        return factories;
    }

    void PassRegistry::AddFactory(Factory factory) {
        Factories().push_back(factory);
    }

    std::optional<PassPair> PassProvider::Get(const std::string& name) {
        EnsureInitialized();

        auto it = m_passes.find(name);
        if (it == m_passes.end())
            return std::nullopt;

        return std::make_pair(it->first, it->second.get());
    }

    const PassMap& PassProvider::GetAll() {
        EnsureInitialized();
        return m_passes;
    }

    void PassProvider::AddPass(std::unique_ptr<BaseAnalysisPass>& pass) {
        EnsureInitialized();
        m_passes.emplace(std::string(pass->Name()), std::move(pass));
    }

    void PassProvider::EnsureInitialized() {
        if (!m_initialized)
            Init();
    }

    void PassProvider::Init() {
        if (m_initialized) return;

        for (auto& factory : PassRegistry::Factories()) {
            auto passPtr = factory();
            m_passes.emplace(std::string(passPtr->Name()), std::move(passPtr));
        }

        m_initialized = true;
    }
} // namespace ReWizard
