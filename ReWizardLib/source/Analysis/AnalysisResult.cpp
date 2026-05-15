#include <ReWizard/Analysis/AnalysisResult.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/FileLoader/FileLoader.h>
#include <ReWizard/Disassembler/Disassembler.h>

#include <sstream>
#include <iomanip>

namespace ReWizard {

    AnalysisResult AnalysisResult::FromContext(AnalysisContext* context) {
        AnalysisResult result;
        result.BuildFromContext(context);
        return result;
    }

    void AnalysisResult::BuildFromContext(AnalysisContext* context) {
        if (!context) return;

        auto loader = context->GetLoader();
        auto module = context->GetModule();

        m_root["target"] = context->GetName();
        if (loader) {
            m_root["image_base"] = loader->CurrentImageBase();
            m_root["image_size"] = loader->MappedSize();
        }

        // Functions
        nlohmann::json functions = nlohmann::json::array();
        if (module) {
            for (const auto& fn : module->GetFunctions()) {
                functions.push_back(SerializeFunction(fn.get(), context));
            }
        }
        m_root["functions"] = functions;

        // Symbol table
        nlohmann::json symbols;
        if (module && module->GetSymbolTable()) {
            auto* symTable = module->GetSymbolTable();

            nlohmann::json imports = nlohmann::json::array();
            for (const auto& imp : symTable->GetImports()) {
                imports.push_back({
                    {"name", imp.name},
                    {"dll", imp.dll},
                    {"address", imp.address},
                    {"iat_entry", imp.iatEntry}
                });
            }
            symbols["imports"] = imports;

            nlohmann::json exports = nlohmann::json::array();
            for (const auto& exp : symTable->GetExports()) {
                exports.push_back({
                    {"name", exp.name},
                    {"address", exp.address},
                    {"ordinal", exp.ordinal}
                });
            }
            symbols["exports"] = exports;

            nlohmann::json relocs = nlohmann::json::array();
            for (const auto& reloc : symTable->GetRelocations()) {
                relocs.push_back({
                    {"address", reloc.address},
                    {"type", reloc.type}
                });
            }
            symbols["relocations"] = relocs;
        }
        m_root["symbols"] = symbols;

        // Visited addresses
        nlohmann::json visited = nlohmann::json::array();
        for (auto addr : context->GetVisited()) {
            visited.push_back(addr);
        }
        m_root["visited_addresses"] = visited;
    }

    nlohmann::json AnalysisResult::SerializeFunction(Function* fn, AnalysisContext* context) const {
        nlohmann::json j;
        j["name"] = fn->GetName();
        j["start"] = fn->GetStart();
        j["end"] = fn->GetEnd();
        j["is_trampoline"] = fn->IsTrampoline();
        j["marked_for_hybrid"] = fn->IsMarked();
        j["hybrid_verified"] = fn->IsHybridVerified();
        j["contains_indirect_calls"] = fn->ContainsIndirectCalls();
        j["contains_indirect_jumps"] = fn->ContainsIndirectJumps();
        j["has_opaque_predicates"] = fn->HasOpaquePredicates();

        nlohmann::json opaquePreds = nlohmann::json::array();
        for (auto addr : fn->GetOpaquePredicateAddresses()) {
            opaquePreds.push_back(addr);
        }
        j["opaque_predicate_addresses"] = opaquePreds;

        nlohmann::json callSites = nlohmann::json::array();
        for (const auto& [src, dst] : fn->GetCallSites()) {
            callSites.push_back({{"source", src}, {"destination", dst}});
        }
        j["call_sites"] = callSites;

        nlohmann::json blocks = nlohmann::json::array();
        for (const auto& bbPtr : fn->GetBasicBlocks()) {
            blocks.push_back(SerializeBasicBlock(bbPtr.get()));
        }
        j["basic_blocks"] = blocks;

        // Disassembly
        auto& disas = const_cast<Disassembler&>(context->GetDisassembler());
        nlohmann::json disasm = nlohmann::json::array();
        for (const auto& line : fn->GetDisassembly(disas)) {
            disasm.push_back(line);
        }
        j["disassembly"] = disasm;

        return j;
    }

    nlohmann::json AnalysisResult::SerializeBasicBlock(BasicBlock* bb) const {
        nlohmann::json j;
        j["start"] = bb->GetStart();
        j["end"] = bb->GetEnd();
        j["last_insn_addr"] = bb->GetLastInsnAddr();
        j["contains_indirect_calls"] = bb->ContainsIndirectCalls();
        j["contains_indirect_jumps"] = bb->ContainsIndirectJumps();

        nlohmann::json successors = nlohmann::json::array();
        for (auto s : bb->GetSuccessors()) {
            successors.push_back(s);
        }
        j["successors"] = successors;

        nlohmann::json predecessors = nlohmann::json::array();
        for (auto p : bb->GetPredecessors()) {
            predecessors.push_back(p);
        }
        j["predecessors"] = predecessors;

        return j;
    }

    std::string AnalysisResult::ToJson() const {
        return m_root.dump(2);
    }

    std::string AnalysisResult::ToDot() const {
        std::ostringstream oss;
        oss << "digraph CFG {\n";
        oss << "  rankdir=TB;\n";
        oss << "  node [shape=box];\n\n";

        auto& functions = m_root["functions"];
        for (const auto& fn : functions) {
            std::string fnName = fn["name"];
            // Sanitize name for DOT
            std::string safeName;
            for (char c : fnName) {
                if (std::isalnum(c) || c == '_') safeName += c;
                else safeName += '_';
            }

            oss << "  subgraph cluster_" << safeName << " {\n";
            oss << "    label=\"" << fnName << " (0x" << std::hex << fn["start"].get<uintptr_t>() << ")\";\n";
            oss << "    style=filled;\n";
            oss << "    color=lightgrey;\n";

            auto& blocks = fn["basic_blocks"];
            for (const auto& bb : blocks) {
                uintptr_t start = bb["start"];
                uintptr_t end = bb["end"];
                oss << "    \"" << safeName << "_0x" << std::hex << start << "\" [label=\"0x" << start << " - 0x" << end << "\"];\n";
            }

            for (const auto& bb : blocks) {
                uintptr_t start = bb["start"];
                for (const auto& succ : bb["successors"]) {
                    uintptr_t succAddr = succ.get<uintptr_t>();
                    // Find which BB contains the successor
                    std::string targetNode;
                    bool found = false;
                    for (const auto& targetBb : blocks) {
                        if (succAddr >= targetBb["start"].get<uintptr_t>() && succAddr <= targetBb["end"].get<uintptr_t>()) {
                            targetNode = safeName + "_0x" + std::to_string(targetBb["start"].get<uintptr_t>());
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        // Cross-function edge or unresolved
                        oss << "    \"" << safeName << "_0x" << std::hex << start << "\" -> \"0x" << succAddr << "\" [color=red];\n";
                    } else {
                        oss << "    \"" << safeName << "_0x" << std::hex << start << "\" -> \"" << targetNode << "\";\n";
                    }
                }
            }

            oss << "  }\n\n";
        }

        oss << "}\n";
        return oss.str();
    }

    std::string AnalysisResult::ToText() const {
        std::ostringstream oss;
        oss << "Analysis Result for: " << m_root.value("target", "unknown") << "\n";
        oss << "Image Base: 0x" << std::hex << m_root.value("image_base", 0) << "\n";
        oss << "Image Size: 0x" << m_root.value("image_size", 0) << std::dec << "\n\n";

        auto& functions = m_root["functions"];
        oss << "Functions (" << functions.size() << "):\n";
        oss << std::string(60, '-') << "\n";

        for (const auto& fn : functions) {
            oss << "\n" << fn["name"].get<std::string>() << ":\n";
            oss << "  Start: 0x" << std::hex << fn["start"].get<uintptr_t>() << "\n";
            oss << "  End:   0x" << fn["end"].get<uintptr_t>() << std::dec << "\n";
            oss << "  Basic Blocks: " << fn["basic_blocks"].size() << "\n";
            oss << "  Call Sites:   " << fn["call_sites"].size() << "\n";

            if (fn["is_trampoline"].get<bool>())
                oss << "  [TRAMPOLINE]\n";
            if (fn["marked_for_hybrid"].get<bool>())
                oss << "  [MARKED FOR HYBRID ANALYSIS]\n";
            if (fn["hybrid_verified"].get<bool>())
                oss << "  [HYBRID VERIFIED]\n";
            if (fn["has_opaque_predicates"].get<bool>())
                oss << "  [OPAQUE PREDICATES DETECTED]\n";

            for (const auto& line : fn["disassembly"]) {
                oss << line.get<std::string>() << "\n";
            }
        }

        auto& symbols = m_root["symbols"];
        if (!symbols.is_null()) {
            oss << "\n" << std::string(60, '-') << "\n";
            oss << "Symbols:\n";
            oss << "  Imports:      " << symbols["imports"].size() << "\n";
            oss << "  Exports:      " << symbols["exports"].size() << "\n";
            oss << "  Relocations:  " << symbols["relocations"].size() << "\n";
        }

        return oss.str();
    }

}
