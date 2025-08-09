#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>


namespace ReWizard {
    static std::unique_ptr<FileLoader> MakeLoader(const std::string& target);
    static bool LoadBinary(std::unique_ptr<FileLoader>& loader);
    static ArchPair GetArch(const std::unique_ptr<FileLoader>& loader);
    static Disassembler& GetDisassembler(const ArchPair& arch);

    std::unique_ptr<AnalysisContext> AnalysisContext::Create(const std::string& target) {
        auto loader = MakeLoader(target);
        if (!loader) return nullptr;

        if (!LoadBinary(loader)) {
            loader.reset();
            return nullptr;
        }

        auto arch = GetArch(loader);
        if (arch.first == ZYDIS_MACHINE_MODE_MAX_VALUE || arch.second == ZYDIS_STACK_WIDTH_MAX_VALUE)
            return nullptr;

        auto& disassembler = GetDisassembler(arch);

        auto ctx = std::unique_ptr<AnalysisContext>(new AnalysisContext(std::move(loader), nullptr, disassembler));
        if (!ctx) return nullptr;

        auto module = Module::Create(ctx.get());
        if (!module) {
            ctx->m_loader.reset();
            return nullptr;
        }

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

    std::unique_ptr<FileLoader> MakeLoader(const std::string& target) {
        auto loader = FileLoader::Create(target);
        if (!loader || loader->Status() != FileLoaderStatus::Success)
            return nullptr;
        return loader;
    }

    ArchPair GetArch(const std::unique_ptr<FileLoader>& loader) {
        if (!loader || loader->Binary()->format() == LIEF::Binary::FORMATS::UNKNOWN)
            return {};

        return loader->Arch();
    }

    bool LoadBinary(std::unique_ptr<FileLoader>& loader) { return loader && loader->Load(); }
    Disassembler& GetDisassembler(const ArchPair& arch) { return Disassembler::Get(arch.first, arch.second); }

}
