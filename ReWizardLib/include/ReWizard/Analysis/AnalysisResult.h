#ifndef ANALYSIS_RESULT_H
#define ANALYSIS_RESULT_H

#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace ReWizard {
    class AnalysisContext;
    class Module;
    class Function;
    class BasicBlock;

    class AnalysisResult {
    public:
        static AnalysisResult FromContext(AnalysisContext* context);

        std::string ToJson() const;
        std::string ToDot() const;
        std::string ToText() const;

    private:
        AnalysisResult() = default;
        void BuildFromContext(AnalysisContext* context);
        nlohmann::json SerializeFunction(Function* fn, AnalysisContext* context) const;
        nlohmann::json SerializeBasicBlock(BasicBlock* bb) const;

        nlohmann::json m_root;
    };
}

#endif
