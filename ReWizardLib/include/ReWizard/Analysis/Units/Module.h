#ifndef UNITS_MODULE_H
#define UNITS_MODULE_H

#include <ReWizard/FileLoader/FileLoader.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/Analysis/AnalysisManager.h>

#include <memory>
#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace ReWizard {
	class AnalysisContext;

	using InstructionCollection = std::map<uintptr_t, std::unique_ptr<ExtendedInstruction>>;
	using PtrFunction = std::unique_ptr<Function>;
	using FunctionCollection = std::vector<PtrFunction>;

	class Module {
	public:
		~Module() = default;

		static std::unique_ptr<Module> Create(AnalysisContext* context);

		PtrFunction CreateFunction(const std::string& name = "") {
			return Function::Create(this, name);
		}

		Function* AddFunction(PtrFunction fn);

		const std::string GetPath() const;

		FileLoader* GetLoader() const;

		FunctionCollection& GetFunctions()  {
			return m_functions;
		}

		Function* GetFunctionForAddress(uintptr_t address);

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

		SymbolTable* GetSymbolTable() { return &m_symbolTable; }
		const SymbolTable* GetSymbolTable() const { return &m_symbolTable; }

	private:
		Module(AnalysisContext* context);

		AnalysisContext* m_context{nullptr};
		InstructionCollection m_instructions;
		FunctionCollection m_functions;
		std::map<uintptr_t, Function*> m_functionByStart;
		SymbolTable m_symbolTable;
	};

}

#endif
