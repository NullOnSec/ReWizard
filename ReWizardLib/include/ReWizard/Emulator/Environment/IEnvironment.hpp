#ifndef EMULATOR_IENVIRONMENT_HPP
#define EMULATOR_IENVIRONMENT_HPP

#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Emulator/Emulator.h>

namespace ReWizard {


	class IEnvironment : public Emulator {
	public:
		IEnvironment(AnalysisContext* context) : m_context(context) {};

		virtual ~IEnvironment() = default;

		virtual bool InitializeEnvironment() = 0;
		virtual bool AnalyzeFunction(Function* function) = 0;
		virtual bool AnalyzeAll() = 0;

		enum class Status {
			EnvironmentReady,
			EnvironmentInitializationFailure
		};

		Status GetStatus() { return m_status; }

	protected:
		AnalysisContext* m_context{ nullptr };
		Status			 m_status{ Status::EnvironmentInitializationFailure };
	};
}

#endif
