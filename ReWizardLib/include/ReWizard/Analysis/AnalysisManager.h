#ifndef ANALYSIS_MANAGER_H
#define ANALYSIS_MANAGER_H

#include <ReWizard/Disassembler/Disassembler.h>
#include <ReWizard/FileLoader/FileLoader.h>
#include <ReWizard/Analysis/PassManager.h>

namespace ReWizard {
	class AnalysisContext;

	class AnalysisManager {
	public:
		static std::unique_ptr<AnalysisManager> Create(std::unique_ptr<AnalysisContext>& context);
		static std::unique_ptr<AnalysisManager> Create(const std::unique_ptr<AnalysisContext>& context);

		FileLoader* Loader();
		const FileLoader* Loader() const;

		std::string Name();
		const std::string Name() const;

		void Run() { m_passManager.RunAllAsync(m_context.get()); }

	private:
		std::unique_ptr<AnalysisContext>	m_context;
		AnalysisPassManager					m_passManager;

	private:
		AnalysisManager(const std::unique_ptr<AnalysisContext>& context);
		AnalysisManager(std::unique_ptr<AnalysisContext>& context);
	};

}

#endif
