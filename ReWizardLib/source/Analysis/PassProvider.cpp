#include <ReWizard/Analysis/PassProvider.h>

/*
    All the passes must be included in this TU to ensure
    that they are automatically registered
*/
#include <ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h>
#include <ReWizard/Analysis/Passes/ImportAnalysisPass.h>
#include <ReWizard/Analysis/Passes/DataFlowAnalysisPass.h>
#include <ReWizard/Analysis/Passes/AbstractInterpretationPass.h>
#include <ReWizard/Analysis/Passes/HybridAnalysisPass.h>
#include <ReWizard/Analysis/Passes/OpaquePredicatePass.h>
#include <ReWizard/Analysis/Passes/IRLiftingPass.hpp>
#include <ReWizard/Analysis/Passes/ConstantFoldingPass.hpp>

#include <spdlog/spdlog.h>

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
        if (m_initialized) 
            return;

        for (auto& factory : PassRegistry::Factories()) {
            auto passPtr = factory();
            m_passes.emplace(std::string(passPtr->Name()), std::move(passPtr));
        }

        m_initialized = true;
    }

    template <typename PassType>
    bool PassRegistrar<PassType>::Register() {
        spdlog::debug("Registering pass: {}", PassType().Name().data());
        PassRegistry::AddFactory(&PassRegistrar<PassType>::Create);
        return true;
    };
} // namespace ReWizard
