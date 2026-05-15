#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace ReWizard {

    struct ImportedSymbol {
        std::string name;
        std::string dll;
        uintptr_t address;
        uintptr_t iatEntry;
    };

    struct ExportedSymbol {
        std::string name;
        uintptr_t address;
        uint16_t ordinal;
    };

    struct RelocationEntry {
        uintptr_t address;
        uintptr_t type;
    };

    class SymbolTable {
    public:
        SymbolTable() = default;
        ~SymbolTable() = default;

        void AddImport(const ImportedSymbol& sym);
        void AddExport(const ExportedSymbol& sym);
        void AddRelocation(const RelocationEntry& reloc);

        const std::vector<ImportedSymbol>& GetImports() const { return m_imports; }
        const std::vector<ExportedSymbol>& GetExports() const { return m_exports; }
        const std::vector<RelocationEntry>& GetRelocations() const { return m_relocations; }

        const ImportedSymbol* GetImportByAddress(uintptr_t address) const;
        const ImportedSymbol* GetImportByName(const std::string& name) const;
        const ExportedSymbol* GetExportByAddress(uintptr_t address) const;
        const ExportedSymbol* GetExportByName(const std::string& name) const;

        void Clear();

    private:
        std::vector<ImportedSymbol> m_imports;
        std::vector<ExportedSymbol> m_exports;
        std::vector<RelocationEntry> m_relocations;

        std::map<uintptr_t, size_t> m_importByAddr;
        std::map<std::string, size_t> m_importByName;
        std::map<uintptr_t, size_t> m_exportByAddr;
        std::map<std::string, size_t> m_exportByName;
    };

}

#endif
