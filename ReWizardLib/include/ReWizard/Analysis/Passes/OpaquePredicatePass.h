#ifndef OPAQUE_PREDICATE_PASS_H
#define OPAQUE_PREDICATE_PASS_H

#include <ReWizard/Analysis/PassProvider.h>

namespace ReWizard {
    class AnalysisContext;

    class OpaquePredicatePass : public PassRegistrar<OpaquePredicatePass> {
    public:
        explicit OpaquePredicatePass()
            : PassRegistrar<OpaquePredicatePass>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~OpaquePredicatePass() = default;

        bool PreRun(AnalysisContext* context) override;
        bool Run(AnalysisContext* context) override;
        bool PostRun(AnalysisContext* context) override;
        std::string_view Name() const override { return "OpaquePredicatePass"; }
    };
}

#endif
