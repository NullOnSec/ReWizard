#include <ReWizard/Analysis/Units/SymbolTable.h>

namespace ReWizard {

    void SymbolTable::AddImport(const ImportedSymbol& sym) {
        size_t idx = m_imports.size();
        m_imports.push_back(sym);
        m_importByAddr[sym.address] = idx;
        m_importByName[sym.name] = idx;
    }

    void SymbolTable::AddExport(const ExportedSymbol& sym) {
        size_t idx = m_exports.size();
        m_exports.push_back(sym);
        m_exportByAddr[sym.address] = idx;
        m_exportByName[sym.name] = idx;
    }

    void SymbolTable::AddRelocation(const RelocationEntry& reloc) {
        m_relocations.push_back(reloc);
    }

    const ImportedSymbol* SymbolTable::GetImportByAddress(uintptr_t address) const {
        auto it = m_importByAddr.find(address);
        if (it != m_importByAddr.end())
            return &m_imports[it->second];
        return nullptr;
    }

    const ImportedSymbol* SymbolTable::GetImportByName(const std::string& name) const {
        auto it = m_importByName.find(name);
        if (it != m_importByName.end())
            return &m_imports[it->second];
        return nullptr;
    }

    const ExportedSymbol* SymbolTable::GetExportByAddress(uintptr_t address) const {
        auto it = m_exportByAddr.find(address);
        if (it != m_exportByAddr.end())
            return &m_exports[it->second];
        return nullptr;
    }

    const ExportedSymbol* SymbolTable::GetExportByName(const std::string& name) const {
        auto it = m_exportByName.find(name);
        if (it != m_exportByName.end())
            return &m_exports[it->second];
        return nullptr;
    }

    void SymbolTable::Clear() {
        m_imports.clear();
        m_exports.clear();
        m_relocations.clear();
        m_importByAddr.clear();
        m_importByName.clear();
        m_exportByAddr.clear();
        m_exportByName.clear();
    }

}
