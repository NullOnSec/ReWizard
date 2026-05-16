#ifndef XREF_MANAGER_H
#define XREF_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace ReWizard {

    class AnalysisDatabase;

    struct Xref {
        uintptr_t from;
        uintptr_t to;
        std::string type;

        bool operator==(const Xref& other) const {
            return from == other.from && to == other.to && type == other.type;
        }
    };

    struct XrefHash {
        std::size_t operator()(const Xref& x) const noexcept {
            std::size_t h1 = std::hash<uintptr_t>{}(x.from);
            std::size_t h2 = std::hash<uintptr_t>{}(x.to);
            std::size_t h3 = std::hash<std::string>{}(x.type);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    class XrefManager {
    public:
        XrefManager() = default;
        ~XrefManager() = default;

        void AddXref(uintptr_t from, uintptr_t to, const std::string& type);
        void RemoveXref(uintptr_t from, uintptr_t to, const std::string& type);
        void Clear();

        std::vector<Xref> GetXrefsTo(uintptr_t address) const;
        std::vector<Xref> GetXrefsFrom(uintptr_t address) const;

        std::vector<Xref> GetCallsTo(uintptr_t address) const;
        std::vector<Xref> GetCallsFrom(uintptr_t address) const;
        std::vector<Xref> GetJumpsTo(uintptr_t address) const;
        std::vector<Xref> GetJumpsFrom(uintptr_t address) const;
        std::vector<Xref> GetDataRefsTo(uintptr_t address) const;

        size_t GetXrefCountTo(uintptr_t address) const;
        size_t GetXrefCountFrom(uintptr_t address) const;

        bool HasXrefsTo(uintptr_t address) const;
        bool HasXrefsFrom(uintptr_t address) const;

        void SaveTo(AnalysisDatabase* db) const;
        void LoadFrom(AnalysisDatabase* db);

    private:
        std::unordered_multimap<uintptr_t, Xref> m_byFrom;
        std::unordered_multimap<uintptr_t, Xref> m_byTo;
        std::unordered_set<Xref, XrefHash> m_all;
    };

}

#endif
