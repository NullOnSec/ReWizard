#ifndef ANALYSIS_PASS_PROVIDER_H
#define ANALYSIS_PASS_PROVIDER_H

#include <ReWizard/Analysis/Passes/BasePass.hpp>
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <map>


namespace ReWizard {

    using PassMap = std::map<std::string, std::unique_ptr<BaseAnalysisPass>>;
    using PassPair = std::pair<std::string, BaseAnalysisPass*>;

    class PassRegistry {
    public:
        using Factory = std::unique_ptr<BaseAnalysisPass>(*)();
        static std::vector<Factory>& Factories();

        static void AddFactory(Factory factory);
    };

    template <typename PassType>
    class PassRegistrar : public BaseAnalysisPass {
    public:
        static std::unique_ptr<BaseAnalysisPass> Create() {
            return std::make_unique<PassType>();
        }

    protected:
        static inline bool Registered = []() {
			//spdlog::info("Registering pass: {}", PassType().Name().data());
            PassRegistry::AddFactory(&PassRegistrar<PassType>::Create);
            return true;
        }();
    };

    class PassProvider {
    public:
        static std::optional<PassPair> Get(const std::string& name);
        static const PassMap& GetAll();
        static void AddPass(std::unique_ptr<BaseAnalysisPass>& pass);

    private:
        static void EnsureInitialized();
        static void Init();

        static inline PassMap m_passes{};
        static inline bool m_initialized{ false };
    };

} // namespace ReWizard

#endif
