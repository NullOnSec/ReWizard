#ifndef CONSTANT_FOLDING_PASS_H
#define CONSTANT_FOLDING_PASS_H

#include <ReWizard/Analysis/PassProvider.h>

namespace ReWizard {

    // Runs LLVM constant propagation (SCCP) and dead code elimination
    // on the LLVM IR produced by IRLiftingPass.
    // This is the primary deobfuscation pass: it folds opaque constants,
    // eliminates dead branches, and simplifies control flow.
    class ConstantFoldingPass : public PassRegistrar<ConstantFoldingPass> {
    public:
        ConstantFoldingPass()
            : PassRegistrar<ConstantFoldingPass>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~ConstantFoldingPass() override = default;

        bool PreRun(AnalysisContext* context) override;
        bool Run(AnalysisContext* context) override;
        bool PostRun(AnalysisContext* context) override;
        std::string_view Name() const override { return "ConstantFoldingPass"; }
        std::vector<std::string_view> Dependencies() const override { return { "IRLiftingPass" }; }
    };

}

#endif
