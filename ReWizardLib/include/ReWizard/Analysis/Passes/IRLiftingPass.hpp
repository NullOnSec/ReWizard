#ifndef IR_LIFTING_PASS_H
#define IR_LIFTING_PASS_H

#include <ReWizard/Analysis/PassProvider.h>
#include <ReWizard/IR/Lifter.h>
#include <vector>
#include <memory>

namespace ReWizard {

    // Lifts all basic blocks in a function to LLVM IR.
    // This pass should run after StaticControlFlowRebuilder so that
    // basic blocks exist and are populated with instructions.
    class IRLiftingPass : public PassRegistrar<IRLiftingPass> {
    public:
        IRLiftingPass()
            : PassRegistrar<IRLiftingPass>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~IRLiftingPass() override = default;

        bool PreRun(AnalysisContext* context) override;
        bool Run(AnalysisContext* context) override;
        bool PostRun(AnalysisContext* context) override;
        std::string_view Name() const override { return "IRLiftingPass"; }
        std::vector<std::string_view> Dependencies() const override { return { "StaticControlFlowRebuilder" }; }

    private:
        std::unique_ptr<Lifter> lifter_;
    };

}

#endif
