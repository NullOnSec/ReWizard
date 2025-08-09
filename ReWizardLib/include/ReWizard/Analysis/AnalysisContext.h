#ifndef ANALYSIS_CONTEXT_H
#define ANALYSIS_CONTEXT_H

#include <ReWizard/Disassembler/Disassembler.h>
#include <ReWizard/FileLoader/FileLoader.h>
#include <ReWizard/Analysis/Units/Module.h>

#include <string>
#include <cstdint>
#include <memory>
#include <set>

namespace ReWizard {
	class Module;

	class AnalysisContext {
	public:
		static std::unique_ptr<AnalysisContext> Create(const std::string& target);
		~AnalysisContext() = default;

		FileLoader* GetLoader() { return m_loader.get(); }
		Module* GetModule() { return m_module.get(); }
		std::string GetName() { return m_targetName; }
		const std::string& GetName() const { return m_targetName; }
		std::set<uintptr_t>& GetVisited() { return m_visited; }
		const std::set<uintptr_t>& GetVisited() const { return m_visited; }

	private:
		AnalysisContext(std::unique_ptr<FileLoader> loader, std::unique_ptr<Module> module, Disassembler& disassembler);
		AnalysisContext(std::unique_ptr<FileLoader>& loader, std::unique_ptr<Module>& module, Disassembler& disassembler);

	private:
		std::unique_ptr<FileLoader> m_loader;
		std::unique_ptr<Module> m_module;
		std::string m_targetName;
		Disassembler& m_disassembler;
		std::set<uintptr_t> m_visited;
	};
}

#endif
