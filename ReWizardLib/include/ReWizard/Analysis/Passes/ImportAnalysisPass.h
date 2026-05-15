#ifndef IMPORT_ANALYSIS_PASS_H
#define IMPORT_ANALYSIS_PASS_H

#include <ReWizard/Analysis/PassProvider.h>

namespace ReWizard {
    class AnalysisContext;

    class ImportAnalysisPass : public PassRegistrar<ImportAnalysisPass> {
    public:
        explicit ImportAnalysisPass()
            : PassRegistrar<ImportAnalysisPass>()
        { m_type = BaseAnalysisPass::Type::PEPass; }

        ~ImportAnalysisPass() = default;

        bool PreRun(AnalysisContext* context) override;
        bool Run(AnalysisContext* context) override;
        bool PostRun(AnalysisContext* context) override;
        std::string_view Name() const override { return "ImportAnalysisPass"; }
    };
}

#endif
