#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>


namespace ReWizard {
	std::unique_ptr<AnalysisContext> AnalysisContext::Create(const std::string& target) {
		auto loader = FileLoader::Create(target);

		if (!loader || (loader && loader->Status() != FileLoaderStatus::Success)) {
			loader.reset(nullptr);
			return nullptr;
		}

		if (!loader->Load()) {
			loader.reset(nullptr);
			return nullptr;
		}

		auto arch = loader->Arch();
		if (loader->Binary()->format() == LIEF::Binary::FORMATS::UNKNOWN) {
			return nullptr;
		}

		auto& disassembler = Disassembler::Get(arch.first, arch.second);
		auto ctx = std::unique_ptr<AnalysisContext>(new AnalysisContext(std::move(loader), nullptr, disassembler));
		if (!ctx)
			return nullptr;

		auto module = Module::Create(ctx.get());
		if (!module)
			return nullptr;

		ctx->m_module = std::move(module);
		ctx->m_targetName = target;

		return ctx;
	}

	AnalysisContext::AnalysisContext(std::unique_ptr<FileLoader>& loader, std::unique_ptr<Module>& module, Disassembler& disassembler)
		: m_loader(std::move(loader)), m_module(std::move(module)), m_targetName(), m_disassembler(disassembler) {
	}

	AnalysisContext::AnalysisContext(std::unique_ptr<FileLoader> loader, std::unique_ptr<Module> module, Disassembler& disassembler)
		: m_loader(std::move(loader)), m_module(std::move(module)), m_targetName(), m_disassembler(disassembler) {
	}

}
