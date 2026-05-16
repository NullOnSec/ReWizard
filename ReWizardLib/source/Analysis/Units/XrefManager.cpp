#include <ReWizard/Analysis/Units/XrefManager.h>
#include <ReWizard/Database/AnalysisDatabase.h>

namespace ReWizard {

    void XrefManager::AddXref(uintptr_t from, uintptr_t to, const std::string& type) {
        Xref x{ from, to, type };
        if (m_all.insert(x).second) {
            m_byFrom.emplace(from, x);
            m_byTo.emplace(to, x);
        }
    }

    void XrefManager::RemoveXref(uintptr_t from, uintptr_t to, const std::string& type) {
        Xref x{ from, to, type };
        auto it = m_all.find(x);
        if (it == m_all.end()) return;

        m_all.erase(it);

        auto range = m_byFrom.equal_range(from);
        for (auto i = range.first; i != range.second; ++i) {
            if (i->second.to == to && i->second.type == type) {
                m_byFrom.erase(i);
                break;
            }
        }

        range = m_byTo.equal_range(to);
        for (auto i = range.first; i != range.second; ++i) {
            if (i->second.from == from && i->second.type == type) {
                m_byTo.erase(i);
                break;
            }
        }
    }

    void XrefManager::Clear() {
        m_all.clear();
        m_byFrom.clear();
        m_byTo.clear();
    }

    std::vector<Xref> XrefManager::GetXrefsTo(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byTo.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            results.push_back(it->second);
        }
        return results;
    }

    std::vector<Xref> XrefManager::GetXrefsFrom(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byFrom.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            results.push_back(it->second);
        }
        return results;
    }

    std::vector<Xref> XrefManager::GetCallsTo(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byTo.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.type == "call") {
                results.push_back(it->second);
            }
        }
        return results;
    }

    std::vector<Xref> XrefManager::GetCallsFrom(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byFrom.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.type == "call") {
                results.push_back(it->second);
            }
        }
        return results;
    }

    std::vector<Xref> XrefManager::GetJumpsTo(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byTo.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.type == "jump") {
                results.push_back(it->second);
            }
        }
        return results;
    }

    std::vector<Xref> XrefManager::GetJumpsFrom(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byFrom.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.type == "jump") {
                results.push_back(it->second);
            }
        }
        return results;
    }

    std::vector<Xref> XrefManager::GetDataRefsTo(uintptr_t address) const {
        std::vector<Xref> results;
        auto range = m_byTo.equal_range(address);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.type == "data") {
                results.push_back(it->second);
            }
        }
        return results;
    }

    size_t XrefManager::GetXrefCountTo(uintptr_t address) const {
        return m_byTo.count(address);
    }

    size_t XrefManager::GetXrefCountFrom(uintptr_t address) const {
        return m_byFrom.count(address);
    }

    bool XrefManager::HasXrefsTo(uintptr_t address) const {
        return m_byTo.find(address) != m_byTo.end();
    }

    bool XrefManager::HasXrefsFrom(uintptr_t address) const {
        return m_byFrom.find(address) != m_byFrom.end();
    }

    void XrefManager::SaveTo(AnalysisDatabase* db) const {
        if (!db) return;
        for (const auto& x : m_all) {
            db->SaveXref(x.from, x.to, x.type);
        }
    }

    void XrefManager::LoadFrom(AnalysisDatabase* db) {
        if (!db) return;
        Clear();
        auto all = db->QueryAllXrefs();
        for (const auto& rec : all) {
            AddXref(rec.fromAddr, rec.toAddr, rec.type);
        }
    }

}
