#include <ReWizard/Analysis/Passes/AbstractInterpretationPass.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Disassembler/Disassembler.h>

#include <spdlog/spdlog.h>

#define UNUSED(x) (void)x

namespace ReWizard {

    bool AbstractInterpretationPass::PreRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool AbstractInterpretationPass::PostRun(AnalysisContext* context) {
        UNUSED(context);
        return true;
    }

    bool AbstractInterpretationPass::Run(AnalysisContext* context) {
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

    void AbstractInterpretationPass::AnalyzeFunction(Function* function, AnalysisContext* context) {
        if (!function || function->GetBasicBlocks().empty())
            return;

        IntervalDomain currentDomain;
        auto& bbs = function->GetBasicBlocks();

        for (auto& bbPtr : bbs) {
            auto* bb = bbPtr.get();
            currentDomain = AnalyzeBasicBlock(bb, context, currentDomain);
        }
    }

    IntervalDomain AbstractInterpretationPass::AnalyzeBasicBlock(BasicBlock* bb, AnalysisContext* context, const IntervalDomain& incoming) {
        IntervalDomain domain = incoming;
        auto module = context->GetModule();
        auto* function = bb->GetFunction();
        uintptr_t addr = bb->GetStart();
        ExtendedInstruction* prevInsn = nullptr;

        while (addr < bb->GetEnd()) {
            auto* insn = module->GetInstruction(addr);
            if (!insn) break;

            // Check for opaque predicates before applying this instruction
            // (so we use the domain state before this instruction modifies it)
            if (insn->Instruction().meta.category == ZYDIS_CATEGORY_COND_BR) {
                auto pred = EvaluateBranch(insn, prevInsn, domain);
                if (pred == PredicateResult::AlwaysTrue || pred == PredicateResult::AlwaysFalse) {
                    function->SetHasOpaquePredicates(true);
                    function->GetOpaquePredicateAddresses().insert(insn->Address());
                    function->GetOpaquePredicateResults()[insn->Address()] = (pred == PredicateResult::AlwaysTrue);
                    spdlog::debug("AbstractInterpretationPass: detected opaque predicate at 0x{:x} ({})",
                                  insn->Address(),
                                  pred == PredicateResult::AlwaysTrue ? "always true" : "always false");
                }
            }

            ApplyInstruction(insn, domain);
            prevInsn = insn;
            addr += insn->Instruction().length;
        }

        return domain;
    }

    Interval AbstractInterpretationPass::GetRegisterInterval(const IntervalDomain& domain, ZydisRegister reg) {
        auto it = domain.find(reg);
        if (it != domain.end()) return it->second;
        return Interval::Top();
    }

    void AbstractInterpretationPass::SetRegisterInterval(IntervalDomain& domain, ZydisRegister reg, const Interval& val) {
        domain[reg] = val;
    }

    void AbstractInterpretationPass::ApplyInstruction(ExtendedInstruction* insn, IntervalDomain& domain) {
        auto& instr = insn->Instruction();
        auto* ops = insn->Operands();

        // mov reg, imm
        if (instr.mnemonic == ZYDIS_MNEMONIC_MOV) {
            const auto& dst = ops[0];
            const auto& src = ops[1];
            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                SetRegisterInterval(domain, dst.reg.value, Interval::Constant(src.imm.value.s));
            }
        }
        // xor reg, reg -> zero
        else if (instr.mnemonic == ZYDIS_MNEMONIC_XOR) {
            const auto& op0 = ops[0];
            const auto& op1 = ops[1];
            if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER && op1.type == ZYDIS_OPERAND_TYPE_REGISTER
                && op0.reg.value == op1.reg.value) {
                SetRegisterInterval(domain, op0.reg.value, Interval::Constant(0));
            }
        }
        // add reg, reg/imm
        else if (instr.mnemonic == ZYDIS_MNEMONIC_ADD) {
            const auto& dst = ops[0];
            const auto& src = ops[1];
            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                auto dstVal = GetRegisterInterval(domain, dst.reg.value);
                Interval srcVal;
                if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) srcVal = Interval::Constant(src.imm.value.s);
                else if (src.type == ZYDIS_OPERAND_TYPE_REGISTER) srcVal = GetRegisterInterval(domain, src.reg.value);
                else srcVal = Interval::Top();
                SetRegisterInterval(domain, dst.reg.value, dstVal.Add(srcVal));
            }
        }
        // sub reg, reg/imm
        else if (instr.mnemonic == ZYDIS_MNEMONIC_SUB) {
            const auto& dst = ops[0];
            const auto& src = ops[1];
            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                auto dstVal = GetRegisterInterval(domain, dst.reg.value);
                Interval srcVal;
                if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) srcVal = Interval::Constant(src.imm.value.s);
                else if (src.type == ZYDIS_OPERAND_TYPE_REGISTER) srcVal = GetRegisterInterval(domain, src.reg.value);
                else srcVal = Interval::Top();
                SetRegisterInterval(domain, dst.reg.value, dstVal.Sub(srcVal));
            }
        }
        // and reg, reg/imm
        else if (instr.mnemonic == ZYDIS_MNEMONIC_AND) {
            const auto& dst = ops[0];
            const auto& src = ops[1];
            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                auto dstVal = GetRegisterInterval(domain, dst.reg.value);
                Interval srcVal;
                if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) srcVal = Interval::Constant(src.imm.value.s);
                else if (src.type == ZYDIS_OPERAND_TYPE_REGISTER) srcVal = GetRegisterInterval(domain, src.reg.value);
                else srcVal = Interval::Top();
                SetRegisterInterval(domain, dst.reg.value, dstVal.And(srcVal));
            }
        }
        // shl reg, imm
        else if (instr.mnemonic == ZYDIS_MNEMONIC_SHL) {
            const auto& dst = ops[0];
            const auto& src = ops[1];
            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                auto dstVal = GetRegisterInterval(domain, dst.reg.value);
                Interval srcVal;
                if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) srcVal = Interval::Constant(src.imm.value.s);
                else srcVal = Interval::Top();
                SetRegisterInterval(domain, dst.reg.value, dstVal.Shl(srcVal));
            }
        }
        // shr reg, imm
        else if (instr.mnemonic == ZYDIS_MNEMONIC_SHR) {
            const auto& dst = ops[0];
            const auto& src = ops[1];
            if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                auto dstVal = GetRegisterInterval(domain, dst.reg.value);
                Interval srcVal;
                if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) srcVal = Interval::Constant(src.imm.value.s);
                else srcVal = Interval::Top();
                SetRegisterInterval(domain, dst.reg.value, dstVal.Shr(srcVal));
            }
        }

        // Invalidate written registers we don't track above
        for (uint8_t i = 0; i < instr.operand_count; ++i) {
            const auto& op = ops[i];
            if (op.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                (op.visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN ||
                 op.actions & ZYDIS_OPERAND_ACTION_WRITE)) {
                auto it = domain.find(op.reg.value);
                if (it != domain.end() && it->second.IsConstant() && it->second.lo == 0 &&
                    (op.reg.value == ZYDIS_REGISTER_RAX || op.reg.value == ZYDIS_REGISTER_EAX ||
                     op.reg.value == ZYDIS_REGISTER_RCX || op.reg.value == ZYDIS_REGISTER_ECX ||
                     op.reg.value == ZYDIS_REGISTER_RDX || op.reg.value == ZYDIS_REGISTER_EDX)) {
                    // xor reg, reg already handled above, skip invalidation
                    continue;
                }
                if (it != domain.end()) {
                    bool tracked = false;
                    switch (instr.mnemonic) {
                    case ZYDIS_MNEMONIC_MOV:
                    case ZYDIS_MNEMONIC_XOR:
                    case ZYDIS_MNEMONIC_ADD:
                    case ZYDIS_MNEMONIC_SUB:
                    case ZYDIS_MNEMONIC_AND:
                    case ZYDIS_MNEMONIC_SHL:
                    case ZYDIS_MNEMONIC_SHR:
                        tracked = true;
                        break;
                    default:
                        break;
                    }
                    if (!tracked) {
                        domain.erase(it);
                    }
                }
            }
        }
    }

    PredicateResult AbstractInterpretationPass::EvaluateBranch(ExtendedInstruction* branchInsn, ExtendedInstruction* prevInsn, const IntervalDomain& domain) {
        if (!prevInsn) return PredicateResult::Unknown;

        auto& branch = branchInsn->Instruction();
        auto& prev = prevInsn->Instruction();
        auto* prevOps = prevInsn->Operands();

        // We handle: test reg, reg / cmp reg, 0 / cmp reg, imm
        bool isTestRegReg = (prev.mnemonic == ZYDIS_MNEMONIC_TEST) &&
            prevOps[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            prevOps[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            prevOps[0].reg.value == prevOps[1].reg.value;

        bool isCmpRegImm = (prev.mnemonic == ZYDIS_MNEMONIC_CMP) &&
            prevOps[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            prevOps[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE;

        if (!isTestRegReg && !isCmpRegImm)
            return PredicateResult::Unknown;

        ZydisRegister reg = prevOps[0].reg.value;
        auto regInterval = GetRegisterInterval(domain, reg);
        if (!regInterval.IsConstant())
            return PredicateResult::Unknown;

        int64_t val = regInterval.lo;
        auto pred = PredicateResult::Unknown;

        switch (branch.mnemonic) {
        case ZYDIS_MNEMONIC_JZ:
            pred = (val == 0) ? PredicateResult::AlwaysTrue : PredicateResult::AlwaysFalse;
            break;
        case ZYDIS_MNEMONIC_JNZ:
            pred = (val != 0) ? PredicateResult::AlwaysTrue : PredicateResult::AlwaysFalse;
            break;
        case ZYDIS_MNEMONIC_JS:
            pred = (val < 0) ? PredicateResult::AlwaysTrue : PredicateResult::AlwaysFalse;
            break;
        case ZYDIS_MNEMONIC_JNS:
            pred = (val >= 0) ? PredicateResult::AlwaysTrue : PredicateResult::AlwaysFalse;
            break;
        default:
            break;
        }

        return pred;
    }

}
