#include <ReWizard/Hybrid/SimpleTraceReader.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace ReWizard {

    bool SimpleTraceReader::Load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            spdlog::warn("SimpleTraceReader: unable to open {}", path);
            return false;
        }

        try {
            nlohmann::json j;
            f >> j;

            if (!j.is_array()) {
                spdlog::warn("SimpleTraceReader: expected JSON array at root");
                return false;
            }

            m_records.clear();
            m_pcIndex.clear();
            m_records.reserve(j.size());
            for (const auto& entry : j) {
                TraceRecord rec;
                if (entry.contains("pc")) {
                    std::string pcStr = entry["pc"].get<std::string>();
                    rec.pc = std::stoull(pcStr, nullptr, 16);
                }

                if (entry.contains("registers") && entry["registers"].is_object()) {
                    for (auto& [key, val] : entry["registers"].items()) {
                        std::string valStr = val.get<std::string>();
                        uintptr_t regVal = std::stoull(valStr, nullptr, 16);
                        ZydisRegister reg = ZYDIS_REGISTER_NONE;
                        // Map common register names
                        if (key == "RAX") reg = ZYDIS_REGISTER_RAX;
                        else if (key == "EAX") reg = ZYDIS_REGISTER_EAX;
                        else if (key == "RCX") reg = ZYDIS_REGISTER_RCX;
                        else if (key == "ECX") reg = ZYDIS_REGISTER_ECX;
                        else if (key == "RDX") reg = ZYDIS_REGISTER_RDX;
                        else if (key == "EDX") reg = ZYDIS_REGISTER_EDX;
                        else if (key == "RBX") reg = ZYDIS_REGISTER_RBX;
                        else if (key == "EBX") reg = ZYDIS_REGISTER_EBX;
                        else if (key == "RSP") reg = ZYDIS_REGISTER_RSP;
                        else if (key == "ESP") reg = ZYDIS_REGISTER_ESP;
                        else if (key == "RBP") reg = ZYDIS_REGISTER_RBP;
                        else if (key == "EBP") reg = ZYDIS_REGISTER_EBP;
                        else if (key == "RSI") reg = ZYDIS_REGISTER_RSI;
                        else if (key == "ESI") reg = ZYDIS_REGISTER_ESI;
                        else if (key == "RDI") reg = ZYDIS_REGISTER_RDI;
                        else if (key == "EDI") reg = ZYDIS_REGISTER_EDI;
                        else if (key == "R8")  reg = ZYDIS_REGISTER_R8;
                        else if (key == "R8D") reg = ZYDIS_REGISTER_R8D;
                        else if (key == "R9")  reg = ZYDIS_REGISTER_R9;
                        else if (key == "R9D") reg = ZYDIS_REGISTER_R9D;
                        else if (key == "R10") reg = ZYDIS_REGISTER_R10;
                        else if (key == "R10D") reg = ZYDIS_REGISTER_R10D;
                        else if (key == "R11") reg = ZYDIS_REGISTER_R11;
                        else if (key == "R11D") reg = ZYDIS_REGISTER_R11D;
                        else if (key == "R12") reg = ZYDIS_REGISTER_R12;
                        else if (key == "R12D") reg = ZYDIS_REGISTER_R12D;
                        else if (key == "R13") reg = ZYDIS_REGISTER_R13;
                        else if (key == "R13D") reg = ZYDIS_REGISTER_R13D;
                        else if (key == "R14") reg = ZYDIS_REGISTER_R14;
                        else if (key == "R14D") reg = ZYDIS_REGISTER_R14D;
                        else if (key == "R15") reg = ZYDIS_REGISTER_R15;
                        else if (key == "R15D") reg = ZYDIS_REGISTER_R15D;
                        else if (key == "RIP") reg = ZYDIS_REGISTER_RIP;
                        else if (key == "EIP") reg = ZYDIS_REGISTER_EIP;

                        if (reg != ZYDIS_REGISTER_NONE) {
                            rec.registers[reg] = regVal;
                        }
                    }
                }

                if (entry.contains("memory") && entry["memory"].is_array()) {
                    for (const auto& memEntry : entry["memory"]) {
                        MemoryAccess ma;
                        if (memEntry.contains("addr")) {
                            std::string addrStr = memEntry["addr"].get<std::string>();
                            ma.address = std::stoull(addrStr, nullptr, 16);
                        }
                        if (memEntry.contains("val")) {
                            std::string valStr = memEntry["val"].get<std::string>();
                            ma.value = std::stoull(valStr, nullptr, 16);
                        }
                        if (memEntry.contains("write")) {
                            ma.isWrite = memEntry["write"].get<bool>();
                        }
                        rec.memoryAccesses.push_back(ma);
                    }
                }

                m_pcIndex[rec.pc].push_back(m_records.size());
                m_records.push_back(std::move(rec));
            }

            spdlog::info("SimpleTraceReader: loaded {} records from {}", m_records.size(), path);
            return true;
        } catch (const std::exception& e) {
            spdlog::warn("SimpleTraceReader: JSON parse error: {}", e.what());
            return false;
        }
    }

    std::vector<const TraceRecord*> SimpleTraceReader::GetRecordsForPC(uintptr_t pc) const {
        std::vector<const TraceRecord*> result;
        auto it = m_pcIndex.find(pc);
        if (it != m_pcIndex.end()) {
            result.reserve(it->second.size());
            for (size_t idx : it->second) {
                result.push_back(&m_records[idx]);
            }
        }
        return result;
    }

}
