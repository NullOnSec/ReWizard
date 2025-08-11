#ifndef EMULATOR_IENVIRONMENT_HPP
#define EMULATOR_IENVIRONMENT_HPP

#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Emulator/Emulator.h>

namespace ReWizard {

	class IEnvironment : public Emulator {
	public:
		IEnvironment(AnalysisContext* context) : m_context(context) { };


	protected:
		AnalysisContext* m_context;
	}

}

#endif
