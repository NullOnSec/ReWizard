#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>
#include <optional>
#include <spdlog/spdlog.h>

namespace ReWizard {
    static std::unique_ptr<FileLoader> MakeLoader(const std::string& target);
    static bool LoadBinary(std::unique_ptr<FileLoader>& loader);
    static std::optional<ArchPair> GetArch(const std::unique_ptr<FileLoader>& loader);
    static Disassembler& GetDisassemblerRef(const ArchPair& arch);

    std::unique_ptr<AnalysisContext> AnalysisContext::Create(const std::string& target) {
        auto loader = MakeLoader(target);
        if (!loader)
            return nullptr;

        if (!LoadBinary(loader)) {
            loader.reset();
            spdlog::error("Unable to map target!");
            return nullptr;
        }

        auto arch = GetArch(loader);
        if (!arch.has_value()) {
            // should not return null in the future, raw binaries should be supported
            return nullptr;
        }

        auto& disassembler = GetDisassemblerRef(arch.value());

        auto ctx = std::unique_ptr<AnalysisContext>(new AnalysisContext(std::move(loader), nullptr, disassembler));
        if (!ctx) {
            spdlog::error("Fatal error: Unable to create AnalysisContext!");
            return nullptr;
        }

        auto module = Module::Create(ctx.get());
        if (!module) {
            ctx->m_loader.reset();
            spdlog::error("Fatal error: Unable to create Units::Module!");
            return nullptr;
        }

        ctx->m_module = std::move(module);
        ctx->m_targetName = target;

        return std::move(ctx);
    }

	AnalysisContext::AnalysisContext(std::unique_ptr<FileLoader>& loader, std::unique_ptr<Module>& module, Disassembler& disassembler)
		: m_loader(std::move(loader)), m_module(std::move(module)), m_targetName(), m_disassembler(disassembler) {
	}

	AnalysisContext::AnalysisContext(std::unique_ptr<FileLoader> loader, std::unique_ptr<Module> module, Disassembler& disassembler)
		: m_loader(std::move(loader)), m_module(std::move(module)), m_targetName(), m_disassembler(disassembler) {
	}

    std::unique_ptr<FileLoader> MakeLoader(const std::string& target) {
        auto loader = FileLoader::Create(target);
        if (!loader || loader->Status() != FileLoaderStatus::Success) {
            
            return nullptr;
        }
        return loader;
    }

    std::optional<ArchPair> GetArch(const std::unique_ptr<FileLoader>& loader) {
        if (!loader || loader->Binary()->format() == LIEF::Binary::FORMATS::UNKNOWN) {
            spdlog::error("Target {} is not any supported executable formats!", loader->Name());
            return std::nullopt;
        }

        return loader->Arch();
    }

    bool LoadBinary(std::unique_ptr<FileLoader>& loader) { return loader && loader->Load(); }
    Disassembler& GetDisassemblerRef(const ArchPair& arch) { return Disassembler::Get(arch.first, arch.second); }

}
