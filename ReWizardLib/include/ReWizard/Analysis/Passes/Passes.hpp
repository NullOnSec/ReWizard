#ifndef ANALYSIS_PASSES_HPP
#define ANALYSIS_PASSES_HPP

#include <ReWizard/Analysis/Passes/BasePass.hpp>
#include <ReWizard/Analysis/Passes/StaticGenericPass.h>
#include <cstdint>
#include <optional>
#include <string>
#include <memory>
#include <array>
#include <map>

namespace ReWizard {

    constexpr inline uint32_t SystemPassCount = 1;

    // System-level passes, initialized at compile time
    inline std::array<std::unique_ptr<BaseAnalysisPass>, SystemPassCount> SystemPasses{
        std::make_unique<StaticGenericPass>()
    };

    using PassMap = std::map<std::string, std::unique_ptr<BaseAnalysisPass>>;
    using PassPair = std::pair<std::string, BaseAnalysisPass*>;

    class PassProvider {
    public:
        // Return a pass by its name (moves it out of the provider)
        static std::optional<PassPair> Get(const std::string& name) {
            EnsureInitialized();

            auto it = m_passes.find(name);
            if (it == m_passes.end())
                return std::nullopt;

            return std::make_pair(it->first, it->second.get());
        }

        // Return all passes (moves the whole map)
        static const PassMap& GetAll() {
            EnsureInitialized();
            return m_passes;
        }

        // Insert a new pass dynamically
        static void AddPass(std::unique_ptr<BaseAnalysisPass>& pass) {
            EnsureInitialized();
            m_passes.emplace(std::string(pass->Name()), std::move(pass));
        }

    private:
        static inline void EnsureInitialized() {
            if (!m_initialized)
                Init();
        }

        static void Init() {
            if (m_initialized) return;

            for (auto& pass : SystemPasses) {
                m_passes.emplace(std::string(pass->Name()), std::move(pass));
            }
            m_initialized = true;
        }

        static inline PassMap m_passes{};
        static inline bool m_initialized{ false };
    };

} // namespace ReWizard

#endif
