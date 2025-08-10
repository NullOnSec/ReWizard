#include <ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>

#include <queue>
#include <set>

#define UNUSED(x) (void)x

namespace ReWizard {
	
	bool StaticControlFlowRebuilder::PreRun(AnalysisContext* context) { UNUSED(context); return true; }
	bool StaticControlFlowRebuilder::PostRun(AnalysisContext* context) { UNUSED(context); return true; }

	bool StaticControlFlowRebuilder::Run(AnalysisContext* context) {
		if (!context) 
			return false;
		auto loader = context->GetLoader();
		auto module = context->GetModule();

		auto ep = loader->Binary()->entrypoint();
		auto buffer = loader->Raw();

		auto entrypointFN = module->CreateFunction("_entrypoint");


		return true;
	}
	

	
}
