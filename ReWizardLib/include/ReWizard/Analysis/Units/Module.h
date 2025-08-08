#ifndef UNITS_MODULE_H
#define UNITS_MODULE_H

#include <ReWizard/FileLoader/FileLoader.h>

#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Manager.h>


#include <memory>
#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace ReWizard {

	using InstructionCollection = std::map<uintptr_t, std::unique_ptr<ExtendedInstruction>>;
	using PtrFunction = std::unique_ptr<Function>;
	using FunctionCollection = std::vector<PtrFunction>;

	class Module {
	public:
		~Module() {
			for (auto& [_, insn] : m_instructions) {
				if (insn) 
					insn.reset(nullptr);
			}
		}

		static std::unique_ptr<Module> Create(AnalysisContext* context);

		PtrFunction CreateFunction(const std::string& name = "") {
			return Function::Create(this, name);
		}

		Function* AddFunction(PtrFunction fn) {
			m_functions.push_back(std::move(fn));
			return m_functions.back().get();
		}

		const std::string& GetPath() const {
			return m_context->GetName();
		}

		FileLoader* GetLoader() const {
			return m_context->GetLoader();
		}

		FunctionCollection& GetFunctions()  {
			return m_functions;
		}

		Function* GetFunctionForAddress(uintptr_t address) {
			for (auto& fn : m_functions) {
				if (address >= fn->GetStart() && address <= fn->GetEnd())
					return fn.get();
			}
			return nullptr;
		}

 		InstructionCollection& GetInstructions() {
			return m_instructions;
		}

		ExtendedInstruction* GetInstruction(uintptr_t address) {
			auto it = m_instructions.find(address);
			if (it == m_instructions.end())
				return nullptr;
			return it->second.get();
		}

		void AddInstruction(std::unique_ptr<ExtendedInstruction>& insn) {
			if (!insn) return;
			if (m_instructions.find(insn->Address()) == m_instructions.end()) {
				m_instructions.insert({ insn->Address(), std::move(insn) });
			}
		}

	private:
		Module(AnalysisContext* context);

		AnalysisContext* m_context{nullptr};
		InstructionCollection m_instructions;
		FunctionCollection m_functions;
	};

}

#endif
