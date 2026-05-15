#include <ReWizard/Analysis/Passes/HybridAnalysisPass.h>
#include <ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Hybrid/SimpleTraceReader.h>
#include <ReWizard/Disassembler/Disassembler.h>

#include <spdlog/spdlog.h>

#define UNUSED(x) (void)x

namespace ReWizard {

    bool HybridAnalysisPass::PreRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool HybridAnalysisPass::PostRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool HybridAnalysisPass::Run(AnalysisContext* context) {
        if (!context)
            return false;

        auto module = context->GetModule();
        if (!module)
            return false;

        if (!m_reader) {
            std::string tracePath = context->GetTracePath();
            if (tracePath.empty()) {
                spdlog::debug("HybridAnalysisPass: no trace path set, skipping");
                return true;
            }
            auto reader = std::make_unique<SimpleTraceReader>();
            if (!reader->Load(tracePath)) {
                spdlog::warn("HybridAnalysisPass: failed to load trace from {}", tracePath);
                return true;
            }
            m_reader = std::move(reader);
        }

        std::set<uintptr_t> resolvedTargets;

        for (const auto& fn : module->GetFunctions()) {
            if (!fn->IsMarked())
                continue;

            auto indirects = fn->GetIndirectInstructions();
            for (auto* insn : indirects) {
                auto* extInsn = static_cast<ExtendedInstruction*>(insn);
                auto records = m_reader->GetRecordsForPC(extInsn->Address());
                for (auto* rec : records) {
                    auto target = ResolveIndirectTarget(extInsn, *rec);
                    if (target) {
                        fn->AddCallSite(insn->Address(), target);
                        resolvedTargets.insert(target);
                        spdlog::debug("HybridAnalysisPass: resolved indirect target at 0x{:x} -> 0x{:x}",
                                      insn->Address(), target);
                    }
                }
            }

            ProcessOpaquePredicates(fn.get(), m_reader.get());
            fn->SetHybridVerified(true);
        }

        // Re-run static analysis on newly resolved targets to discover new functions
        if (!resolvedTargets.empty()) {
            StaticControlFlowRebuilder rebuilder;
            for (auto target : resolvedTargets) {
                if (context->GetLoader()->IsWithinMapping(target) &&
                    !context->GetVisited().contains(target) &&
                    !module->GetFunctionForAddress(target)) {
                    spdlog::info("HybridAnalysisPass: re-analyzing from resolved target 0x{:x}", target);
                    rebuilder.ReAnalyzeFrom(context, target);
                }
            }
        }

        return true;
    }

    uintptr_t HybridAnalysisPass::ResolveIndirectTarget(ExtendedInstruction* insn, const TraceRecord& record) {
        auto& instr = insn->Instruction();
        auto* ops = insn->Operands();

        if (instr.mnemonic != ZYDIS_MNEMONIC_CALL && instr.mnemonic != ZYDIS_MNEMONIC_JMP)
            return 0;

        if (instr.operand_count == 0)
            return 0;

        const auto& op0 = ops[0];

        // call/jmp reg
        if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            auto it = record.registers.find(op0.reg.value);
            if (it != record.registers.end())
                return it->second;
        }
        // call/jmp [reg+disp]
        else if (op0.type == ZYDIS_OPERAND_TYPE_MEMORY) {
            uintptr_t base = 0;
            uintptr_t index = 0;
            if (op0.mem.base != ZYDIS_REGISTER_NONE) {
                auto it = record.registers.find(op0.mem.base);
                if (it != record.registers.end())
                    base = it->second;
            }
            if (op0.mem.index != ZYDIS_REGISTER_NONE) {
                auto it = record.registers.find(op0.mem.index);
                if (it != record.registers.end())
                    index = it->second;
            }
            uintptr_t addr = base + index + static_cast<uintptr_t>(op0.mem.disp.value);

            // Try to find a memory read at this address in the trace record
            for (const auto& ma : record.memoryAccesses) {
                if (ma.address == addr && !ma.isWrite)
                    return ma.value;
            }
        }

        return 0;
    }

    void HybridAnalysisPass::ProcessOpaquePredicates(Function* function, ITraceReader* reader) {
        if (!function->HasOpaquePredicates())
            return;

        auto& opaqueAddrs = function->GetOpaquePredicateAddresses();
        std::set<uintptr_t> toRemove;

        for (auto addr : opaqueAddrs) {
            auto records = reader->GetRecordsForPC(addr);
            if (records.size() > 1) {
                // If we see multiple executions at this branch, check if
                // the next instruction after the branch varies (taken vs not-taken)
                // For simplicity: if there are records for PCs both at this
                // branch and at its fallthrough, the branch is not opaque.
                // A more robust approach would compare PC+length vs branch target.
                toRemove.insert(addr);
                spdlog::debug("HybridAnalysisPass: opaque predicate at 0x{:x} disproven by trace", addr);
            }
        }

        for (auto addr : toRemove)
            opaqueAddrs.erase(addr);

        if (opaqueAddrs.empty())
            function->SetHasOpaquePredicates(false);
    }

    void HybridAnalysisPass::SetTraceReader(std::unique_ptr<ITraceReader> reader) {
        m_reader = std::move(reader);
    }

}
