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

	Function* Module::AddFunction(PtrFunction fn) {
		Function* raw = fn.get();
		m_functions.push_back(std::move(fn));
		m_functionByStart[raw->GetStart()] = raw;
		return raw;
	}

	Function* Module::GetFunctionForAddress(uintptr_t address) {
		auto it = m_functionByStart.upper_bound(address);
		if (it == m_functionByStart.begin())
			return nullptr;
		--it;
		Function* fn = it->second;
		if (address >= fn->GetStart() && address <= fn->GetEnd())
			return fn;
		return nullptr;
	}

}
