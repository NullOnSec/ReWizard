#include <ReWizard/Analysis/Passes/DataFlowAnalysisPass.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/FileLoader/FileLoader.h>

#include <spdlog/spdlog.h>

#define UNUSED(x) (void)x

namespace ReWizard {

    bool DataFlowAnalysisPass::PreRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool DataFlowAnalysisPass::PostRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool DataFlowAnalysisPass::Run(AnalysisContext* context) {
        if (!context)
            return false;

        auto module = context->GetModule();
        if (!module)
            return false;

        for (const auto& fn : module->GetFunctions()) {
            AnalyzeFunction(fn.get(), context);
        }

        return true;
    }

    void DataFlowAnalysisPass::AnalyzeFunction(Function* function, AnalysisContext* context) {
        if (!function || function->GetBasicBlocks().empty())
            return;

        // Simple iterative dataflow: start with empty register state
        RegisterFile currentRegs;
        auto& bbs = function->GetBasicBlocks();

        // For now, just process blocks sequentially (no merge points)
        // A full iterative solver would be needed for loops/branches
        for (auto& bbPtr : bbs) {
            auto* bb = bbPtr.get();
            currentRegs = AnalyzeBasicBlock(bb, context, currentRegs);
        }
    }

    RegisterFile DataFlowAnalysisPass::AnalyzeBasicBlock(BasicBlock* bb, AnalysisContext* context, const RegisterFile& incoming) {
        RegisterFile regs = incoming;
        auto module = context->GetModule();
        auto loader = context->GetLoader();
        uintptr_t addr = bb->GetStart();

        while (addr < bb->GetEnd()) {
            auto* insn = module->GetInstruction(addr);
            if (!insn) break;

            // Try to resolve indirect calls/jumps before updating state
            if (insn->Instruction().meta.category == ZYDIS_CATEGORY_CALL) {
                TryResolveIndirectCall(insn, regs, bb->GetFunction());
            } else if (insn->Instruction().meta.category == ZYDIS_CATEGORY_UNCOND_BR ||
                       insn->Instruction().meta.category == ZYDIS_CATEGORY_COND_BR) {
                TryResolveIndirectJump(insn, regs, bb->GetFunction());
            }

            ApplyInstruction(insn, regs, context);
            addr += insn->Instruction().length;
        }

        return regs;
    }

    void DataFlowAnalysisPass::ApplyInstruction(ExtendedInstruction* insn, RegisterFile& regs, AnalysisContext* context) {
        auto& instr = insn->Instruction();
        auto* operands = insn->Operands();

        // Track mov reg, imm
        if (instr.mnemonic == ZYDIS_MNEMONIC_MOV) {
            const auto& dst = operands[0];
            const auto& src = operands[1];

            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                RegisterState state;
                state.defAddress = insn->Address();
                state.knownValue = src.imm.value.u;
                state.isKnown = true;
                regs[dst.reg.value] = state;
            }
        }
        // Track lea reg, [rip+disp] or lea reg, [abs]
        else if (instr.mnemonic == ZYDIS_MNEMONIC_LEA) {
            const auto& dst = operands[0];
            const auto& src = operands[1];

            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_MEMORY) {
                uintptr_t addr = 0;
                bool resolved = false;

                if (src.mem.base == ZYDIS_REGISTER_RIP && src.mem.index == ZYDIS_REGISTER_NONE) {
                    addr = insn->Address() + instr.length + src.mem.disp.value;
                    resolved = true;
                } else if (src.mem.base == ZYDIS_REGISTER_NONE && src.mem.index == ZYDIS_REGISTER_NONE) {
                    addr = static_cast<uintptr_t>(src.mem.disp.value);
                    resolved = true;
                }

                if (resolved) {
                    RegisterState state;
                    state.defAddress = insn->Address();
                    state.knownValue = addr;
                    state.isKnown = true;
                    regs[dst.reg.value] = state;
                }
            }
        }
        // Track mov reg, [rip+disp] (memory read)
        else if (instr.mnemonic == ZYDIS_MNEMONIC_MOV) {
            const auto& dst = operands[0];
            const auto& src = operands[1];

            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_MEMORY) {
                uintptr_t memAddr = 0;
                bool resolved = false;

                if (src.mem.base == ZYDIS_REGISTER_RIP && src.mem.index == ZYDIS_REGISTER_NONE) {
                    memAddr = insn->Address() + instr.length + src.mem.disp.value;
                    resolved = true;
                } else if (src.mem.base == ZYDIS_REGISTER_NONE && src.mem.index == ZYDIS_REGISTER_NONE) {
                    memAddr = static_cast<uintptr_t>(src.mem.disp.value);
                    resolved = true;
                }

                if (resolved && context->GetLoader()->IsWithinMapping(memAddr)) {
                    // Read the pointer value from memory
                    uintptr_t value = 0;
                    memcpy(&value, reinterpret_cast<void*>(memAddr), sizeof(uintptr_t));

                    RegisterState state;
                    state.defAddress = insn->Address();
                    state.knownValue = value;
                    state.isKnown = true;
                    regs[dst.reg.value] = state;
                }
            }
        }
        // Track xor reg, reg -> zero
        else if (instr.mnemonic == ZYDIS_MNEMONIC_XOR) {
            const auto& op0 = operands[0];
            const auto& op1 = operands[1];
            if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER && op1.type == ZYDIS_OPERAND_TYPE_REGISTER
                && op0.reg.value == op1.reg.value) {
                RegisterState state;
                state.defAddress = insn->Address();
                state.knownValue = 0;
                state.isKnown = true;
                regs[op0.reg.value] = state;
            }
        }

        // TODO: Handle more complex data-flow (arithmetic, pushing/popping, etc.)
        // For now, any instruction that writes a register we don't explicitly track
        // will invalidate that register's known value.
        for (uint8_t i = 0; i < instr.operand_count; ++i) {
            const auto& op = operands[i];
            if (op.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                (op.visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN ||
                 op.actions & ZYDIS_OPERAND_ACTION_WRITE)) {
                // If we didn't explicitly set it above, invalidate it
                auto it = regs.find(op.reg.value);
                if (it != regs.end() && it->second.defAddress != insn->Address()) {
                    regs.erase(it);
                }
            }
        }
    }

    bool DataFlowAnalysisPass::TryResolveIndirectCall(ExtendedInstruction* insn, const RegisterFile& regs, Function* function) {
        const auto& op0 = insn->Operands()[0];
        if (op0.type != ZYDIS_OPERAND_TYPE_REGISTER)
            return false;

        auto it = regs.find(op0.reg.value);
        if (it == regs.end() || !it->second.isKnown)
            return false;

        uintptr_t target = it->second.knownValue;
        if (target == 0)
            return false;

        insn->IndirectValue() = target;
        function->AddCallSite(insn->Address(), target);
        spdlog::debug("DataFlowAnalysisPass: resolved indirect call at 0x{:x} -> 0x{:x}",
                      insn->Address(), target);

        return true;
    }

    bool DataFlowAnalysisPass::TryResolveIndirectJump(ExtendedInstruction* insn, const RegisterFile& regs, Function* function) {
        const auto& op0 = insn->Operands()[0];
        if (op0.type != ZYDIS_OPERAND_TYPE_REGISTER)
            return false;

        auto it = regs.find(op0.reg.value);
        if (it == regs.end() || !it->second.isKnown)
            return false;

        uintptr_t target = it->second.knownValue;
        if (target == 0)
            return false;

        insn->IndirectValue() = target;
        function->AddCallSite(insn->Address(), target);
        spdlog::debug("DataFlowAnalysisPass: resolved indirect jump at 0x{:x} -> 0x{:x}",
                      insn->Address(), target);

        return true;
    }

}
