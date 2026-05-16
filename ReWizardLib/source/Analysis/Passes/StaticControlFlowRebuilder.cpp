#include <ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/XrefManager.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>

#include <ReWizard/Disassembler/Disassembler.h>

#include <spdlog/spdlog.h>

#include <queue>
#include <set>

#define UNUSED(x) (void)x

namespace ReWizard {

    bool StaticControlFlowRebuilder::PreRun(AnalysisContext* context) { UNUSED(context); return true; }
    bool StaticControlFlowRebuilder::PostRun(AnalysisContext* context) { UNUSED(context); return true; }

    void StaticControlFlowRebuilder::ReAnalyzeFrom(AnalysisContext* context, uintptr_t address) {
        if (!context || !address) return;
        StaticPathExplorer(context, false, address, false);
    }

    bool StaticControlFlowRebuilder::Run(AnalysisContext* context) {
        if (!context)
            return false;

        StaticPathExplorer(context);

        return true;
    }

    bool StaticControlFlowRebuilder::IsExecutableAddress(FileLoader* loader, uintptr_t addr) const {
        for (auto& [start, end] : loader->GetExecutableSections()) {
            if (addr >= start && addr < end) return true;
        }
        return false;
    }

    std::set<uintptr_t> StaticControlFlowRebuilder::CollectEntryPoints(AnalysisContext* context) {
        std::set<uintptr_t> entries;
        auto loader = context->GetLoader();
        auto binary = loader->Binary();

        // Entry point
        auto ep = binary->entrypoint();
        if (loader->Binary()->imagebase() != loader->CurrentImageBase())
            ep = (ep - loader->Binary()->imagebase()) + loader->CurrentImageBase();
        if (IsExecutableAddress(loader, ep))
            entries.insert(ep);

        // Exported symbols
        auto* symTable = context->GetModule()->GetSymbolTable();
        if (symTable) {
            for (const auto& exp : symTable->GetExports()) {
                if (exp.address && loader->IsWithinMapping(exp.address) && IsExecutableAddress(loader, exp.address))
                    entries.insert(exp.address);
            }
        }

        return entries;
    }

    void StaticControlFlowRebuilder::StaticPathExplorer(AnalysisContext* context, bool reAnalyzeAll, uintptr_t fromAddress, bool bypassHeur) {
        auto module = context->GetModule();
        auto loader = context->GetLoader();

        std::queue<uintptr_t> functionWork;

        if (reAnalyzeAll || module->GetFunctions().empty()) {
            if (reAnalyzeAll) {
                module->GetFunctions().clear();
                context->GetVisited().clear();
            }

            auto entries = CollectEntryPoints(context);
            for (auto addr : entries)
                functionWork.push(addr);
        } else if (!reAnalyzeAll && fromAddress) {
            functionWork.push(fromAddress);
        }

        bool verbose = true;
        while (!functionWork.empty()) {
            uintptr_t pc = functionWork.front(); functionWork.pop();
            if (context->GetVisited().contains(pc))
                continue;

            // Skip if this address is already inside a known function
            if (module->GetFunctionForAddress(pc))
                continue;

            // Only start functions in executable sections
            if (!IsExecutableAddress(loader, pc)) {
                spdlog::debug("StaticPathExplorer: skipping non-executable entry point 0x{:016x}", pc);
                continue;
            }

            auto fn = module->CreateFunction();
            fn->SetStart(pc);
            fn->SetEnd(pc);

            size_t insnCount = 0, stackModifier = 0;

            AnalyzeFunction(context, fn, pc, &insnCount, &stackModifier, verbose, functionWork);
            if (bypassHeur) {
                fn->MarkForHybridAnalysis();
            } else if (insnCount && (fn->ContainsIndirectCalls() || fn->ContainsIndirectJumps())) {
                // Sound heuristic: if static analysis can't resolve targets,
                // or if opaque predicates are detected, mark for hybrid analysis
                fn->MarkForHybridAnalysis();
            }

            // Enqueue direct call targets as new functions
            for (auto& [src, dest] : fn->GetCallSites()) {
                if (dest && !context->GetVisited().contains(dest)) {
                    if (loader->IsWithinMapping(dest) && !module->GetFunctionForAddress(dest))
                        functionWork.push(dest);
                }
            }

            module->AddFunction(std::move(fn));
        }

        LinearSweepFallback(context, functionWork);
    }

    void StaticControlFlowRebuilder::AnalyzeFunction(AnalysisContext* context, std::unique_ptr<Function>& function, uintptr_t start, size_t* insnCount, size_t* stackModCount, bool verbose, std::queue<uintptr_t>& functionWork) {
        auto module = context->GetModule();
        auto loader = context->GetLoader();
        auto &disassembler = context->GetDisassembler();
        std::queue<uintptr_t> work;

        work.push(start);

        if (verbose) {
            spdlog::info("Disassemblying: {}", function->GetName());
            spdlog::info("");
        }

        const uintptr_t imgBase = loader->CurrentImageBase();
        const uintptr_t imgEnd = imgBase + loader->MappedSize();

        function->SetStart(start);

        size_t localInsnCount = 0;
        size_t localStackModCount = 0;

        ExtendedInstruction* lastInsn = nullptr;

        while (!work.empty()) {
            uintptr_t pc = work.front(); work.pop();
            if (context->GetVisited().contains(pc)) continue;

            std::unique_ptr<BasicBlock> bb;

            while (true) {
                if (context->GetVisited().contains(pc)) break;
                uint8_t* instrPtr = reinterpret_cast<uint8_t*>(pc);
                if (pc < imgBase || pc >= imgEnd)
                    break;

                ExtendedInstruction* insn = nullptr;
                auto tempInsn = disassembler->DisassembleSingle<ExtendedInstruction>(instrPtr, ZYDIS_MAX_INSTRUCTION_LENGTH);
                insn = tempInsn.get();
                module->AddInstruction(tempInsn);

                if (!insn) {
                    if (bb) {
                        bb->SetEnd(pc);
                        bb->SetLastInsnAddr(lastInsn->Address());
                        function->SetEnd(bb->GetEnd());
                        function->SetLastInsnAddr(bb->GetLastInsnAddr());
                    }
                    break;
                }

                if (!bb)
                    bb = function->CreateBasicBlock(pc);

                context->GetVisited().insert(insn->Address());

                if (verbose) {
                    spdlog::info("\t{}", disassembler->InstructionToString(insn, insn->Address()));
                }

                if (!localInsnCount && HandleTrampoline(context, function, bb, insn)) {
                    localInsnCount++;
                    break;
                }

                localInsnCount++;
                if (insn->Instruction().mnemonic == ZYDIS_MNEMONIC_PUSH || insn->Instruction().mnemonic == ZYDIS_MNEMONIC_POP)
                    localStackModCount++;

                if (insn->Instruction().operand_count_visible >= 2) {
                    const auto& lhs = insn->Operands()[0];
                    const auto& rhs = insn->Operands()[1];

                    bool lhsIsStack = (
                        (lhs.type == ZYDIS_OPERAND_TYPE_REGISTER)
                        && (lhs.reg.value == ZYDIS_REGISTER_RSP || lhs.reg.value == ZYDIS_REGISTER_RBP ||
                            lhs.reg.value == ZYDIS_REGISTER_ESP || lhs.reg.value == ZYDIS_REGISTER_EBP)
                    );

                    bool rhsIsImmDwordOrMore = (
                        rhs.type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
                        (rhs.size >= 32)
                    );

                    if (lhsIsStack && rhsIsImmDwordOrMore)
                        localStackModCount++;
                }

                auto cat = insn->Instruction().meta.category;
                auto& op0 = insn->Operands()[0];

                if (cat == ZYDIS_CATEGORY_CALL) {
                    HandleCall(context, insn, pc, function, bb, functionWork);
                } else if (cat == ZYDIS_CATEGORY_COND_BR || cat == ZYDIS_CATEGORY_UNCOND_BR) {
                    HandleBranch(context, insn, pc, function, bb, work, functionWork);
                    bb = nullptr;

                    if (cat == ZYDIS_CATEGORY_UNCOND_BR)
                        break;

                } else if (cat == ZYDIS_CATEGORY_RET) {
                    bb->SetEnd(insn->Address() + insn->Instruction().length);
                    bb->SetLastInsnAddr(insn->Address());

                    function->SetEnd(bb->GetEnd());
                    function->SetLastInsnAddr(bb->GetLastInsnAddr());
                    function->AddBasicBlock(std::move(bb));

                    bb = nullptr;
                    break;
                }

                lastInsn = insn;
                pc += insn->Instruction().length;
            }
            if (bb) function->AddBasicBlock(std::move(bb));
        }
        function->NormalizeBasicBlocks();

        if (insnCount)
            *insnCount = localInsnCount;

        if (stackModCount)
            *stackModCount = localStackModCount;

        if (verbose) { spdlog::info(""); spdlog::info(""); }
    }

    void StaticControlFlowRebuilder::HandleCall(AnalysisContext* context, ExtendedInstruction* insn, uintptr_t pc, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, std::queue<uintptr_t>& functionWork) {
        auto* xrefs = context->GetModule()->GetXrefManager();
        const auto& op0 = insn->Operands()[0];
        if (op0.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
            auto target = pc + insn->Instruction().length + op0.imm.value.u;
            function->AddCallSite(pc, target);
            xrefs->AddXref(pc, target, "call");
            // Enqueue direct call target as a new function entry point
            if (context->GetLoader()->IsWithinMapping(target) && !context->GetVisited().contains(target) && IsExecutableAddress(context->GetLoader(), target))
                functionWork.push(target);
        } else if (op0.type == ZYDIS_OPERAND_TYPE_MEMORY && op0.mem.base == ZYDIS_REGISTER_RIP) {
            auto ripTarget = pc + insn->Instruction().length + op0.mem.disp.value;
            function->AddCallSite(pc, ripTarget);
            xrefs->AddXref(pc, ripTarget, "call");
        } else if (op0.type == ZYDIS_OPERAND_TYPE_MEMORY &&
            op0.mem.base == ZYDIS_REGISTER_NONE &&
            op0.mem.index == ZYDIS_REGISTER_NONE) {
            auto absTarget = static_cast<uintptr_t>(op0.mem.disp.value);
            function->AddCallSite(pc, absTarget);
            xrefs->AddXref(pc, absTarget, "call");
        } else {
            function->AddCallSite(pc, insn->IndirectValue());
            xrefs->AddXref(pc, insn->IndirectValue(), "call");
            insn->IsIndirect() = true;
            bb->SetContainsIndirectCalls(true);
            function->SetContainsIndirectCalls(true);
        }
    }

    void StaticControlFlowRebuilder::HandleBranch(AnalysisContext* context, ExtendedInstruction* insn, uintptr_t pc, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, std::queue<uintptr_t>& work, std::queue<uintptr_t>& functionWork) {
        auto* xrefs = context->GetModule()->GetXrefManager();
        const auto& op0 = insn->Operands()[0];
        uintptr_t imgBase = context->GetLoader()->CurrentImageBase();
        uintptr_t imgEnd = imgBase + context->GetLoader()->MappedSize();
        auto module = context->GetModule();
        bool isUncondBr = (insn->Instruction().meta.category == ZYDIS_CATEGORY_UNCOND_BR);

        if (op0.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
            auto target = pc + insn->Instruction().length + op0.imm.value.u;
            if (context->GetLoader()->IsWithinMapping(target) && IsExecutableAddress(context->GetLoader(), target)) {
                if (isUncondBr) {
                    auto existingFn = module->GetFunctionForAddress(target);
                    if (existingFn) {
                        function->AddCallSite(pc, target);
                        xrefs->AddXref(pc, target, "jump");
                    } else if (!context->GetVisited().contains(target)) {
                        work.push(target);
                        functionWork.push(target);
                        xrefs->AddXref(pc, target, "jump");
                    }
                } else {
                    if (!context->GetVisited().contains(target))
                        work.push(target);
                    xrefs->AddXref(pc, target, "jump");
                }
                bb->AddSuccessor(target);
            } else if (target < imgBase || target >= imgEnd) {
                spdlog::warn("jmp out of bounds 0x{:016x} at 0x{:016x}", target, insn->Address());
            }
        } else if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER || op0.type == ZYDIS_OPERAND_TYPE_MEMORY) {
            insn->IsIndirect() = true;
            function->AddCallSite(pc, insn->IndirectValue());
            xrefs->AddXref(pc, insn->IndirectValue(), "jump");
            function->SetContainsIndirectJumps(true);
            bb->SetContainsIndirectJumps(true);
        }

        if (insn->Instruction().meta.category == ZYDIS_CATEGORY_COND_BR) {
            bb->AddSuccessor(pc + insn->Instruction().length);
            xrefs->AddXref(pc, pc + insn->Instruction().length, "jump");
        }

        bb->SetEnd(pc + insn->Instruction().length);
        bb->SetLastInsnAddr(pc);

        function->SetEnd(bb->GetEnd());
        function->SetLastInsnAddr(bb->GetLastInsnAddr());
        function->AddBasicBlock(std::move(bb));
    }

    bool StaticControlFlowRebuilder::HandleTrampoline(AnalysisContext* context, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, ExtendedInstruction* insn) {
        uintptr_t target = 0;
        if (insn->Instruction().meta.category == ZYDIS_CATEGORY_UNCOND_BR) {
            const auto& op0 = insn->Operands()[0];
            uintptr_t imgBase = context->GetLoader()->CurrentImageBase();
            uintptr_t imgEnd = imgBase + context->GetLoader()->MappedSize();

            if (op0.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                target = insn->Address() + insn->Instruction().length + op0.imm.value.u;
                function->AddCallSite(insn->Address(), target);
            } else if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER || op0.type == ZYDIS_OPERAND_TYPE_MEMORY) {
                insn->IsIndirect() = true;
                bb->SetContainsIndirectJumps(true);
                function->SetContainsIndirectJumps(true);
            }

            bb->SetEnd(insn->Address() + insn->Instruction().length);
            bb->SetLastInsnAddr(insn->Address());

            function->SetEnd(bb->GetEnd());
            function->SetLastInsnAddr(bb->GetLastInsnAddr());
            function->AddBasicBlock(std::move(bb));

            if (target >= imgBase && target < imgEnd && function->GetName() != "__entrypoint") {
                std::stringstream ss;
                ss << "j__" << std::hex << target;
                function->SetName(ss.str());
            }

            function->SetIsTrampoline(true);
            return true;
        }

        return false;
    }

    void StaticControlFlowRebuilder::LinearSweepFallback(AnalysisContext* context, std::queue<uintptr_t>& functionWork) {
        auto loader = context->GetLoader();
        auto module = context->GetModule();
        auto& disassembler = context->GetDisassembler();

        auto execSections = loader->GetExecutableSections();
        for (auto& [start, end] : execSections) {
            uintptr_t addr = start;
            while (addr < end) {
                if (context->GetVisited().contains(addr)) {
                    addr++;
                    continue;
                }

                uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
                auto insn = disassembler->DisassembleSingle<ExtendedInstruction>(ptr, ZYDIS_MAX_INSTRUCTION_LENGTH);
                if (!insn) {
                    addr++;
                    continue;
                }

                // Simple function prologue heuristic: push rbp / mov rbp, rsp
                bool isPrologue = false;
                if (insn->Instruction().mnemonic == ZYDIS_MNEMONIC_PUSH) {
                    const auto& op0 = insn->Operands()[0];
                    if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                        (op0.reg.value == ZYDIS_REGISTER_RBP || op0.reg.value == ZYDIS_REGISTER_EBP)) {
                        isPrologue = true;
                    }
                } else if (insn->Instruction().mnemonic == ZYDIS_MNEMONIC_MOV) {
                    const auto& op0 = insn->Operands()[0];
                    const auto& op1 = insn->Operands()[1];
                    if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                        (op0.reg.value == ZYDIS_REGISTER_RBP || op0.reg.value == ZYDIS_REGISTER_EBP) &&
                        op1.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                        (op1.reg.value == ZYDIS_REGISTER_RSP || op1.reg.value == ZYDIS_REGISTER_ESP)) {
                        isPrologue = true;
                    }
                } else if (insn->Instruction().mnemonic == ZYDIS_MNEMONIC_SUB) {
                    // sub rsp, imm  (common function start on x64)
                    const auto& op0 = insn->Operands()[0];
                    const auto& op1 = insn->Operands()[1];
                    if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                        (op0.reg.value == ZYDIS_REGISTER_RSP || op0.reg.value == ZYDIS_REGISTER_ESP) &&
                        op1.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                        isPrologue = true;
                    }
                }

                if (isPrologue) {
                    if (!module->GetFunctionForAddress(addr) && IsExecutableAddress(loader, addr)) {
                        functionWork.push(addr);
                        spdlog::debug("LinearSweepFallback: found potential prologue at 0x{:x}", addr);
                    }
                }

                addr += insn->Instruction().length;
            }
        }

        // If we found any new candidates, process them
        while (!functionWork.empty()) {
            uintptr_t pc = functionWork.front(); functionWork.pop();
            if (context->GetVisited().contains(pc))
                continue;
            if (module->GetFunctionForAddress(pc))
                continue;

            auto fn = module->CreateFunction();
            fn->SetStart(pc);
            fn->SetEnd(pc);

            size_t insnCount = 0, stackModifier = 0;
            AnalyzeFunction(context, fn, pc, &insnCount, &stackModifier, false, functionWork);

            for (auto& [src, dest] : fn->GetCallSites()) {
                if (dest && !context->GetVisited().contains(dest)) {
                    if (loader->IsWithinMapping(dest) && !module->GetFunctionForAddress(dest))
                        functionWork.push(dest);
                }
            }

            module->AddFunction(std::move(fn));
        }
    }

}
