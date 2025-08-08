#include <ReWizard/Analysis/Passes/StaticGenericPass.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>

#include <queue>
#include <set>

#define UNUSED(x) (void)x

namespace ReWizard {

	bool StaticGenericPass::Run(AnalysisContext* context) {
		if (!context) 
			return false;
		auto loader = context->GetLoader();
		auto module = context->GetModule();

		auto ep = loader->Binary()->entrypoint();
		auto buffer = loader->Raw();

		auto entrypointFN = module->CreateFunction("_entrypoint");


		return true;
	}
	
	bool StaticGenericPass::PreRun(AnalysisContext* context) { UNUSED(context); return true; }
	bool StaticGenericPass::PostRun(AnalysisContext* context) { UNUSED(context); return true; }

	
}
