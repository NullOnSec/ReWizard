#include <ReWizard/Analysis/Passes/StaticControlFlowRebuilder.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/FileLoader/FileLoader.h>

#include <ReWizard/Disassembler/Disassembler.h>

#include <spdlog/spdlog.h>

#include <queue>
#include <set>

#define UNUSED(x) (void)x

namespace ReWizard {
	
	bool StaticControlFlowRebuilder::PreRun(AnalysisContext* context) { UNUSED(context); return true; }
	bool StaticControlFlowRebuilder::PostRun(AnalysisContext* context) { UNUSED(context); return true; }

	bool StaticControlFlowRebuilder::Run(AnalysisContext* context) {
		if (!context) 
			return false;

		StaticPathExplorer(context);

		return true;
	}
	

	void StaticControlFlowRebuilder::StaticPathExplorer(AnalysisContext* context, bool reAnalyzeAll, uintptr_t fromAddress, bool bypassHeur) {
		auto module = context->GetModule();
		auto loader = context->GetLoader();

		std::queue<uintptr_t> work;
		auto visit_callsites = [&](std::unique_ptr<Function>& fn) {
			for (auto& [src, dest] : fn->GetCallSites()) {
				if (dest && !context->GetVisited().contains(dest)) {
					if (loader->IsWithinMapping(dest))
						work.push(dest);
				}
			}
		};

		PtrFunction fn = nullptr;

		if (reAnalyzeAll || module->GetFunctions().empty()) {
			if (reAnalyzeAll) {
				module->GetFunctions().clear();
				context->GetVisited().clear();
			}

			fn = module->CreateFunction("__entrypoint");

			auto start = loader->Binary()->entrypoint();
			if (loader->Binary()->imagebase() != loader->CurrentImageBase())
				start = (start - loader->Binary()->imagebase()) + loader->CurrentImageBase();

			work.push(start);
		} else if (!reAnalyzeAll && fromAddress) {
			work.push(fromAddress);
		}

		bool verbose = true;
		while (!work.empty()) {
			uintptr_t pc = work.front(); work.pop();
			if (context->GetVisited().contains(pc))
				continue;

			if (!fn)
				fn = module->CreateFunction();

			if (fn && !fn->GetStart())
				fn->SetStart(pc);

			if (fn && !fn->GetEnd())
				fn->SetEnd(pc);

			size_t insnCount = 0, stackModifier = 0;

			AnalyzeFunction(context, fn, pc, &insnCount, &stackModifier, verbose);
			if (insnCount && (fn->ContainsIndirectCalls() || fn->ContainsIndirectJumps())
				&& ((stackModifier * 100 / insnCount) > 30 || bypassHeur)) {
				/* vague is_opaque heuristic */
				fn->MarkForHybridAnalysis();
			}

			visit_callsites(fn);

			module->AddFunction(std::move(fn));
			fn = nullptr;
		}
	}

	void StaticControlFlowRebuilder::AnalyzeFunction(AnalysisContext* context, std::unique_ptr<Function>& function, uintptr_t start, size_t* insnCount, size_t* stackModCount, bool verbose) {
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

				//module->AddInstruction(insn);
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
					HandleCall(context, insn, pc, function, bb);
				}
				else if (cat == ZYDIS_CATEGORY_COND_BR || cat == ZYDIS_CATEGORY_UNCOND_BR) {
					HandleBranch(context, insn, pc, function, bb, work);
					bb = nullptr;
					if (cat == ZYDIS_CATEGORY_UNCOND_BR) break;
				}
				else if (cat == ZYDIS_CATEGORY_RET) {
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

	void StaticControlFlowRebuilder::HandleCall(AnalysisContext* context, ExtendedInstruction* insn, uintptr_t pc, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb) {
		const auto& op0 = insn->Operands()[0];
		if (op0.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
			auto target = pc + insn->Instruction().length + op0.imm.value.u;
			function->AddCallSite(pc, target);
		}
		else if (op0.type == ZYDIS_OPERAND_TYPE_MEMORY && op0.mem.base == ZYDIS_REGISTER_RIP) {
			auto ripTarget = pc + insn->Instruction().length + op0.mem.disp.value;
			function->AddCallSite(pc, ripTarget);
		}
		else if (op0.type == ZYDIS_OPERAND_TYPE_MEMORY &&
			op0.mem.base == ZYDIS_REGISTER_NONE &&
			op0.mem.index == ZYDIS_REGISTER_NONE) {
			auto absTarget = static_cast<uintptr_t>(op0.mem.disp.value);
			function->AddCallSite(pc, absTarget);
		} else {
			/* (op0.type == ZYDIS_OPERAND_TYPE_REGISTER || op0.type == ZYDIS_OPERAND_TYPE_MEMORY) */
			function->AddCallSite(pc, insn->IndirectValue());
			insn->IsIndirect() = true;
			bb->SetContainsIndirectCalls(true);
			function->SetContainsIndirectCalls(true);
		}
	}

	void StaticControlFlowRebuilder::HandleBranch(AnalysisContext* context, ExtendedInstruction* insn, uintptr_t pc, std::unique_ptr<Function>& function, std::unique_ptr<BasicBlock>& bb, std::queue<uintptr_t>& work) {
		const auto& op0 = insn->Operands()[0];
		uintptr_t imgBase = context->GetLoader()->CurrentImageBase();
		uintptr_t imgEnd = imgBase + context->GetLoader()->MappedSize();

		if (op0.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
			auto target = pc + insn->Instruction().length + op0.imm.value.u;
			if (context->GetLoader()->IsWithinMapping(target)) {
				if (!context->GetVisited().contains(target)) work.push(target);
				bb->AddSuccessor(target);
			} else if (target < imgBase || target >= imgEnd) {
				spdlog::warn("jmp out of bounds 0x{:016x} at 0x{:016x}", target, insn->Address());
			}
		}
		else if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER || op0.type == ZYDIS_OPERAND_TYPE_MEMORY) {
			insn->IsIndirect() = true;
			function->AddCallSite(pc, insn->IndirectValue());
			bb->SetContainsIndirectJumps(true);
			function->SetContainsIndirectJumps(true);
		}

		if (insn->Instruction().meta.category == ZYDIS_CATEGORY_COND_BR)
			bb->AddSuccessor(pc + insn->Instruction().length);

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
			}
			else if (op0.type == ZYDIS_OPERAND_TYPE_REGISTER || op0.type == ZYDIS_OPERAND_TYPE_MEMORY) {
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
	
}
