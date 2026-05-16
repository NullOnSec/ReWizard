#include <ReWizard/Analysis/Units/SymbolManager.h>
#include <ReWizard/Database/AnalysisDatabase.h>
#include <spdlog/spdlog.h>

namespace ReWizard {

    void SymbolManager::SetName(uintptr_t address, const std::string& name, const std::string& kind) {
        auto& a = m_annotations[address];
        a.name = name;
        a.kind = kind;
    }

    void SymbolManager::SetComment(uintptr_t address, const std::string& text) {
        m_annotations[address].comment = text;
    }

    void SymbolManager::SetType(uintptr_t address, const std::string& type) {
        m_annotations[address].type = type;
    }

    std::optional<std::string> SymbolManager::GetName(uintptr_t address) const {
        auto it = m_annotations.find(address);
        if (it == m_annotations.end()) return std::nullopt;
        return it->second.name;
    }

    std::optional<std::string> SymbolManager::GetComment(uintptr_t address) const {
        auto it = m_annotations.find(address);
        if (it == m_annotations.end()) return std::nullopt;
        return it->second.comment;
    }

    std::optional<std::string> SymbolManager::GetType(uintptr_t address) const {
        auto it = m_annotations.find(address);
        if (it == m_annotations.end()) return std::nullopt;
        return it->second.type;
    }

    std::optional<std::string> SymbolManager::GetKind(uintptr_t address) const {
        auto it = m_annotations.find(address);
        if (it == m_annotations.end()) return std::nullopt;
        return it->second.kind;
    }

    const Annotation* SymbolManager::GetAnnotation(uintptr_t address) const {
        auto it = m_annotations.find(address);
        if (it == m_annotations.end()) return nullptr;
        return &it->second;
    }

    void SymbolManager::Remove(uintptr_t address) {
        m_annotations.erase(address);
    }

    void SymbolManager::Clear() {
        m_annotations.clear();
    }

    void SymbolManager::SaveTo(AnalysisDatabase* db) const {
        if (!db) return;
        for (const auto& [addr, ann] : m_annotations) {
            db->SaveAnnotation(addr, ann.name, ann.comment, ann.type, ann.kind);
        }
    }

    void SymbolManager::LoadFrom(AnalysisDatabase* db) {
        if (!db) return;
        Clear();
        auto records = db->QueryAllAnnotations();
        for (const auto& rec : records) {
            auto& a = m_annotations[rec.address];
            a.name = rec.name;
            a.comment = rec.comment;
            a.type = rec.type;
            a.kind = rec.kind;
        }
    }

    size_t SymbolManager::Count() const {
        return m_annotations.size();
    }

}
