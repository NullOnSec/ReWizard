#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/AnalysisContext.h>

namespace ReWizard {

	std::unique_ptr<Module> Module::Create(AnalysisContext* context) {
		return std::unique_ptr<Module>(new Module(context));
	}

	Module::Module(AnalysisContext* context)
		: m_context(context) { }

	const std::string Module::GetPath() const {
		return m_context->GetName();
	}

	FileLoader* Module::GetLoader() const {
		return m_context->GetLoader();
	}

}
