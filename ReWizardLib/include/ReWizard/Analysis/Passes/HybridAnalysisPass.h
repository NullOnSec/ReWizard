#ifndef HYBRID_ANALYSIS_PASS_H
#define HYBRID_ANALYSIS_PASS_H

#include <ReWizard/Analysis/PassProvider.h>
#include <ReWizard/Hybrid/ITraceReader.h>
#include <string>
#include <cstdint>

namespace ReWizard {
    class AnalysisContext;
    class Function;
    class ExtendedInstruction;
    struct TraceRecord;

    class HybridAnalysisPass : public PassRegistrar<HybridAnalysisPass> {
    public:
        explicit HybridAnalysisPass()
            : PassRegistrar<HybridAnalysisPass>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~HybridAnalysisPass() = default;

        bool PreRun(AnalysisContext* context) override;
        bool Run(AnalysisContext* context) override;
        bool PostRun(AnalysisContext* context) override;
        std::string_view Name() const override { return "HybridAnalysisPass"; }

        void SetTraceReader(std::unique_ptr<ITraceReader> reader);

    private:
        uintptr_t ResolveIndirectTarget(ExtendedInstruction* insn, const TraceRecord& record);
        void ProcessOpaquePredicates(Function* function, ITraceReader* reader);

        std::unique_ptr<ITraceReader> m_reader;
    };
}

#endif
