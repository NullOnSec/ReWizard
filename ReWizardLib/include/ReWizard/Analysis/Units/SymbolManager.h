#ifndef SYMBOL_MANAGER_H
#define SYMBOL_MANAGER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>

namespace ReWizard {

    class AnalysisDatabase;

    struct Annotation {
        std::string name;
        std::string comment;
        std::string type;
        std::string kind;
    };

    class SymbolManager {
    public:
        SymbolManager() = default;
        ~SymbolManager() = default;

        void SetName(uintptr_t address, const std::string& name, const std::string& kind = "unknown");
        void SetComment(uintptr_t address, const std::string& text);
        void SetType(uintptr_t address, const std::string& type);

        std::optional<std::string> GetName(uintptr_t address) const;
        std::optional<std::string> GetComment(uintptr_t address) const;
        std::optional<std::string> GetType(uintptr_t address) const;
        std::optional<std::string> GetKind(uintptr_t address) const;
        const Annotation* GetAnnotation(uintptr_t address) const;

        void Remove(uintptr_t address);
        void Clear();

        void SaveTo(AnalysisDatabase* db) const;
        void LoadFrom(AnalysisDatabase* db);

        size_t Count() const;

    private:
        std::unordered_map<uintptr_t, Annotation> m_annotations;
    };

}

#endif
