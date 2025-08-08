#ifndef STATIC_GENERIC_PASS_H
#define STATIC_GENERIC_PASS_H

#include <ReWizard/Analysis/Passes/BasePass.hpp>
#include <string_view>

namespace ReWizard {
	class AnalysisContext;

	class StaticGenericPass : public BaseAnalysisPass {
	public:
		explicit StaticGenericPass()
			: BaseAnalysisPass() { m_type = BaseAnalysisPass::Type::GenericPass; }

		~StaticGenericPass() = default;

		bool PreRun(AnalysisContext* context) override;
		bool Run(AnalysisContext* context) override;
		bool PostRun(AnalysisContext* context) override;
		std::string_view Name() const override { return "StaticGenericPass"; }
	};

}

#endif
