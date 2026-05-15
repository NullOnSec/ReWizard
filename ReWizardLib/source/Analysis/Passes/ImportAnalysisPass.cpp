#include <ReWizard/Analysis/Passes/ImportAnalysisPass.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/FileLoader/FileLoader.h>

#include <LIEF/PE.hpp>
#include <spdlog/spdlog.h>

#define UNUSED(x) (void)x

namespace ReWizard {

    bool ImportAnalysisPass::PreRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool ImportAnalysisPass::PostRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool ImportAnalysisPass::Run(AnalysisContext* context) {
        if (!context)
            return false;

        auto loader = context->GetLoader();
        if (!loader)
            return false;

        auto pe = dynamic_cast<LIEF::PE::Binary*>(loader->Binary());
        if (!pe) {
            spdlog::debug("ImportAnalysisPass: target is not a PE binary, skipping");
            return true;
        }

        auto module = context->GetModule();
        if (!module)
            return false;

        auto symbolTable = module->GetSymbolTable();
        if (!symbolTable)
            return false;

        symbolTable->Clear();

        const uintptr_t imageBase = loader->CurrentImageBase();

        for (const auto& imp : pe->imports()) {
            const std::string dllName = imp.name();
            for (const auto& entry : imp.entries()) {
                ImportedSymbol sym;
                sym.name = entry.name();
                sym.dll = dllName;
                sym.iatEntry = imageBase + entry.iat_address();

                if (entry.is_ordinal()) {
                    if (sym.name.empty()) {
                        sym.name = dllName + "!Ordinal_" + std::to_string(entry.ordinal());
                    }
                }

                sym.address = sym.iatEntry;
                symbolTable->AddImport(sym);
            }
        }

        auto exports = pe->get_export();
        if (exports) {
            for (const auto& exp : exports->entries()) {
                ExportedSymbol sym;
                sym.name = exp.name();
                sym.address = imageBase + exp.address();
                sym.ordinal = exp.ordinal();
                symbolTable->AddExport(sym);
            }
        }

        for (const auto& reloc : pe->relocations()) {
            for (const auto& entry : reloc.entries()) {
                RelocationEntry r;
                r.address = imageBase + reloc.virtual_address() + entry.position();
                r.type = static_cast<uintptr_t>(entry.type());
                symbolTable->AddRelocation(r);
            }
        }

        spdlog::info("ImportAnalysisPass: found {} imports, {} exports, {} relocations",
                     symbolTable->GetImports().size(),
                     symbolTable->GetExports().size(),
                     symbolTable->GetRelocations().size());

        return true;
    }

}
