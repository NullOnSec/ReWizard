#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Manager.h>

namespace ReWizard {

	std::unique_ptr<Module> Module::Create(AnalysisContext* context) {
		return std::unique_ptr<Module>(new Module(context));
	}

	Module::Module(AnalysisContext* context)
		: m_context(context) { }
}
