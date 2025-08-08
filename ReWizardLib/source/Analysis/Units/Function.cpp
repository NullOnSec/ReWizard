#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <queue>

namespace ReWizard {

    BasicBlock* Function::GetBasicBlockForAddress(uintptr_t address, bool* wasSplit, bool allowSplit) {
        if (!allowSplit) {
            for (auto& block : basicBlocks_) {
                if (address >= block->GetStart() && address < block->GetEnd())
                    return block.get();
            }
            return nullptr;
        }

        for (auto& block : basicBlocks_) {
            if (address == block->GetStart()) return block.get();

            if (address > block->GetStart() && address < block->GetEnd()) {
                PtrBasicBlock newBlk = block->Split(address);
                if (!newBlk)
                    return nullptr;

                BasicBlock* rawPtr = newBlk.get();
                AddBasicBlock(std::move(newBlk));  // Must move!
                if (wasSplit) 
                    *wasSplit = true;
                return rawPtr; // Return valid pointer owned by this->basicBlocks_
            }
        }
        return nullptr;
    }


    void Function::NormalizeBasicBlocks() {
        std::set<uintptr_t> entries;
        for (const auto& bb : basicBlocks_) {
            entries.insert(bb->GetStart());
            for (auto succ : bb->GetSuccessors())
                entries.insert(succ);
        }
        // Collect all fallthroughs
        for (const auto& bb : basicBlocks_) {
            // If bb ends with cond branch, add fallthrough
            auto end = bb->GetEnd();
            if (!bb->GetSuccessors().empty()) {
                for (auto succ : bb->GetSuccessors()) {
                    if (succ > bb->GetStart() && succ < end)
                        entries.insert(succ);
                }
            }
        }

    reset:
        for (auto& bb : basicBlocks_) {
            for (auto entry : entries) {
                if (entry == bb->GetStart()) continue;
                if (entry > bb->GetStart() && entry < bb->GetEnd()) {
                    bool wasSplit = false;
                    auto splitBlk = GetBasicBlockForAddress(entry, &wasSplit);
                    if (wasSplit) goto reset;
                }
            }
            for (auto succ : bb->GetSuccessors()) {
                bool wasSplit = false;
                auto startAddr = bb->GetStart();
                auto succBlk = GetBasicBlockForAddress(succ, &wasSplit);
                if (!succBlk) continue;
                succBlk->AddPredecessor(startAddr);
                if (wasSplit) goto reset;
            }
        }

        BuildCFG();

    }

    void Function::BuildCFG() {
        cfgGraph_.clear();
        bbVertexMap_.clear();

        // Map each BB to a vertex
        for (auto& bbPtr : basicBlocks_) {
            auto* bb = bbPtr.get();
            auto v = boost::add_vertex(BBVertex{ bb }, cfgGraph_);
            bbVertexMap_[bb] = v;
        }

        std::set<BasicBlock*> visited;
        std::queue<BasicBlock*> toVisit;

        auto* entry = GetBasicBlockForAddress(GetStart(), nullptr, false);
        if (!entry) return;

        toVisit.push(entry);

        while (!toVisit.empty()) {
            auto* bb = toVisit.front(); toVisit.pop();
            if (!bb) 
                continue;
            auto fromV = bbVertexMap_[bb];

            // Visit all successors, always add edge
            for (auto succAddr : bb->GetSuccessors()) {
                auto* succBB = GetBasicBlockForAddress(succAddr, nullptr, false);
                if (!succBB) 
                    continue;

                auto it = bbVertexMap_.find(succBB);
                if (it == bbVertexMap_.end()) 
                    continue;

                auto toV = it->second;
                boost::add_edge(fromV, toV, cfgGraph_);

                // Only queue unvisited
                if (!visited.count(succBB))
                    toVisit.push(succBB);
            }
            visited.insert(bb);
        }
    }


    std::string Function::DumpCFGToGraphVizString() {
        std::ostringstream oss;
        boost::write_graphviz(
            oss, cfgGraph_,
            [this](std::ostream& os, const CFGGraph::vertex_descriptor v) {
                auto* bb = cfgGraph_[v].bb;
                os << "[label=\"0x" << std::hex << bb->GetStart() << "\"]";
            }
        );
        return oss.str();
    }

    std::vector<DecodedInstruction*> Function::GetIndirectInstructions() {
        std::vector<DecodedInstruction*> instructions;
        std::set<uintptr_t> visited;
        std::queue<BasicBlock*> work;

        auto* entry = GetBasicBlockForAddress(GetStart());
        if (!entry) return instructions;
        work.push(entry);

        while (!work.empty()) {
            auto* bb = work.front(); work.pop();
            if (!bb || visited.contains(bb->GetStart())) continue;
            visited.insert(bb->GetStart());

            for (uintptr_t addr = bb->GetStart(); addr < bb->GetEnd(); ) {
                
                auto* insn = module_->GetInstruction(addr);
                if (!insn) break;
                if (insn->IsIndirect())
                    instructions.push_back(insn);
                addr += insn->Instruction().length;
            }

            for (auto succ : bb->GetSuccessors()) {
                auto* succBb = GetBasicBlockForAddress(succ);
                if (succBb && !visited.contains(succBb->GetStart()))
                    work.push(succBb);
            }
        }

        return instructions;
    }

    void Function::BasicBlockForEach(const std::function<void(BasicBlock*)>& fn) {
        struct Cmp {
            bool operator()(const BasicBlock* a, const BasicBlock* b) const {
                return a->GetStart() < b->GetStart();
            }
        };
        std::set<BasicBlock*, Cmp> sortedBlocks;

        for (auto& pair : bbVertexMap_) {
            if (pair.first)
                sortedBlocks.insert(pair.first);
        }

        for (auto* bb : sortedBlocks)
            fn(bb);
    }

    struct BbPtrCompare {
        bool operator()(const BasicBlock* a, const BasicBlock* b) const {
            return a->GetStart() < b->GetStart();
        }
    };

    const std::vector<std::string>& Function::GetDisassembly(Disassembler& disas, bool forceRecompute) {

        if (!disassemblyCached_ || forceRecompute) {
            disassemblyCache_.clear();
            std::set<uintptr_t> visited;
            std::set<BasicBlock*, BbPtrCompare> toVisit;

            auto& bbs = GetBasicBlocks();
            if (bbs.empty()) return disassemblyCache_;
            toVisit.insert(bbs.front().get());

            disassemblyCache_.push_back("start " + GetName() + "\n");

            while (!toVisit.empty()) {
                auto* bb = *toVisit.begin();
                toVisit.erase(toVisit.begin());
                if (!bb || visited.count(bb->GetStart()))
                    continue;
                visited.insert(bb->GetStart());

                if (bb->GetStart() != bb->GetEnd() && !IsTrampoline())
                    disassemblyCache_.push_back("\t" + bb->GetName() + ":");
                uintptr_t addr = bb->GetStart();
                while (addr < bb->GetEnd()) {
                    auto* insn = module_->GetInstruction(addr);
                    if (!insn) break;

                    std::ostringstream oss;
                    oss << "\t\t"
                        << std::left << std::setw(64)  // 64 char columns
                        << disas->InstructionToString(insn, insn->Address());

                    if (insn->IsIndirect() && insn->IndirectValue()) {
                        oss << " --> 0x" << std::hex << insn->IndirectValue();
                    }

                    disassemblyCache_.push_back(oss.str());
                    addr += insn->Instruction().length;
                }
                for (auto next : bb->GetSuccessors()) {
                    auto block = GetBasicBlockForAddress(next);
                    if (block) toVisit.insert(block);

                }
                if (!IsTrampoline()) disassemblyCache_.push_back("\n");
            }
            disassemblyCache_.push_back("end " + GetName() + "\n");
            disassemblyCached_ = true;
        }
        return disassemblyCache_;
    }



}

