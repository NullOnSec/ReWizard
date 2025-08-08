#ifndef ANALYSIS_MANAGER_H
#define ANALYSIS_MANAGER_H

#include <ReWizard/FileLoader/FileLoader.h>


namespace ReWizard {

	class BaseAnalysisPass;
	class Module;

	class AnalysisContext {
	public:
		static std::unique_ptr<AnalysisContext> Create(const std::string &target);
		~AnalysisContext() = default;

		FileLoader* GetLoader() { return m_loader.get(); }
		Module* GetModule() { return m_module.get(); }
		std::string GetName() { return m_targetName; }
		const std::string& GetName() const { return m_targetName; }


	private:
		AnalysisContext(FileLoader* loader, Module* module, Disassembler& disassembler);
		AnalysisContext(std::unique_ptr<FileLoader> loader, std::unique_ptr<Module> module, Disassembler& disassembler);
		AnalysisContext(std::unique_ptr<FileLoader>& loader, std::unique_ptr<Module>& module, Disassembler& disassembler);

	private:
		std::unique_ptr<FileLoader> m_loader;
		std::unique_ptr<Module> m_module;
		std::string m_targetName;
		Disassembler& m_disassembler;
	};

	class AnalysisManager {
	public:
		static std::unique_ptr<AnalysisManager> Create(std::unique_ptr<AnalysisContext>& context);
		static std::unique_ptr<AnalysisManager> Create(const std::unique_ptr<AnalysisContext>& context);

		FileLoader* Loader() { return m_context->GetLoader(); }
		const FileLoader* Loader() const { return m_context->GetLoader(); }

		std::string Name() { return m_context->GetName(); }
		const std::string Name() const { return m_context->GetName(); }

	private:
		std::unique_ptr<AnalysisContext> m_context;

	private:
		AnalysisManager(const std::unique_ptr<AnalysisContext>& context);
		AnalysisManager(std::unique_ptr<AnalysisContext>& context);
	};

	class BaseAnalysisPass {
	public:
		BaseAnalysisPass();

		enum class Type {
			GenericPass,
			PEPass,
			MachOPass,
			ELFPass,
		};

		virtual bool PreRun() = 0;
		virtual bool Run() = 0;
		virtual bool PostRun() = 0;

		Type GetType() { return m_type; }

	protected:
		Type m_type = Type::GenericPass;
	};

}

#endif