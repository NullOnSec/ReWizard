#ifndef WIN32_EMULATION_ENVIRONMENT_H
#define WIN32_EMULATION_ENVIRONMENT_H

#include <ReWizard/Emulator/Environment/IEnvironment.hpp>

namespace ReWizard {
	class Win32Environment : public IEnvironment {
	public:
		Win32Environment(AnalysisContext* ctx) : IEnvironment(ctx) {}

		bool InitializeEnvironment() override;
		bool AnalyzeFunction(Function* function) override;
		bool AnalyzeAll() override;

	};
}

#endif
