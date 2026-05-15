#ifndef STATIC_GENERIC_PASS_H
#define STATIC_GENERIC_PASS_H

#include <ReWizard/Analysis/PassProvider.h>
#include <string_view>
#include <queue>
#include <set>

namespace ReWizard {
    class AnalysisContext;
    class Function;
    class BasicBlock;
    class ExtendedInstruction;

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

    private:
        void StaticPathExplorer(AnalysisContext* context, bool reAnalyzeAll = false, uintptr_t fromAddress = 0, bool bypassHeur = false);
        void AnalyzeFunction(AnalysisContext* context, std::unique_ptr<Function>& function, uintptr_t start, size_t* insnCount, size_t* stackModCount, bool verbose);
        void HandleCall(AnalysisContext* context, ExtendedInstruction* insn, uintptr_t pc, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, std::queue<uintptr_t>& functionWork);
        void HandleBranch(AnalysisContext* context, ExtendedInstruction* insn, uintptr_t pc, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, std::queue<uintptr_t>& work);
        bool HandleTrampoline(AnalysisContext* context, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, ExtendedInstruction* insn);

        std::set<uintptr_t> CollectEntryPoints(AnalysisContext* context);
        void LinearSweepFallback(AnalysisContext* context, std::queue<uintptr_t>& functionWork);
    };
}

#endif
