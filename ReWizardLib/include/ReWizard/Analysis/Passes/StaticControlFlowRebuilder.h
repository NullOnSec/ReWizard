#ifndef STATIC_GENERIC_PASS_H
#define STATIC_GENERIC_PASS_H

#include <ReWizard/Analysis/PassProvider.h>
#include <string_view>

namespace ReWizard {
    class AnalysisContext;

    class StaticControlFlowRebuilder : public PassRegistrar<StaticControlFlowRebuilder> {
    public:
        explicit StaticControlFlowRebuilder()
            : PassRegistrar<StaticControlFlowRebuilder>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~StaticControlFlowRebuilder() = default;

        bool PreRun(AnalysisContext* context) override;
        bool Run(AnalysisContext* context) override;
        bool PostRun(AnalysisContext* context) override;
        std::string_view Name() const override { return "StaticControlFlowRebuilder"; }
    };
    
}

#endif
