#include <ReWizard/Emulator/Environment/Win32Impl/Win32Environment.h>

namespace ReWizard {

	bool Win32Environment::InitializeEnvironment() {
		return true;
	}
	
	bool Win32Environment::AnalyzeFunction(Function* function) {
		return true;
	}

	bool Win32Environment::AnalyzeAll() {
		return true;
	}

}
