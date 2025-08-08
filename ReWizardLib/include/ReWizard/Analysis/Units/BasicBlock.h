#ifndef UNITS_BASIC_BLOCK_H
#define UNITS_BASIC_BLOCK_H

#include <ReWizard/Disassembler/Disassembler.h>

#include <memory>
#include <cstdint>
#include <map>
#include <sstream>
#include <ostream>

namespace ReWizard {

	using InstructionCollection = std::map<uintptr_t, std::unique_ptr<ExtendedInstruction>>;
	using InstructionCollectionIt = InstructionCollection::iterator;

	class Function;

	class BasicBlock {
	public:
		using PtrBasicBlock = std::unique_ptr<BasicBlock>;
		static PtrBasicBlock Create(Function* func, uintptr_t start, const std::string& name = "loc_") {
			return std::unique_ptr<BasicBlock>(new BasicBlock(func, name, start));
		}

		PtrBasicBlock Split(uintptr_t address);

		~BasicBlock() = default;

		uintptr_t GetStart() const { return startAddress_; }
		uintptr_t GetEnd() const { return endAddress_; }
		uintptr_t GetLastInsnAddr() const { return lastInsnAddr_; }


		void SetStart(uintptr_t start) {
			startAddress_ = start;
		}

		void SetEnd(uintptr_t end) {
			endAddress_ = end;
		}

		void SetLastInsnAddr(uintptr_t addr) {
			lastInsnAddr_ = addr;
		}

		const std::string& GetName() const { return name_; }
		Function* GetFunction() const { return func_; }

		bool ContainsIndirectCalls() const {
			return containsIndirectCalls_;
		}

		bool ContainsIndirectJumps() const {
			return containsIndirectJumps_;
		}

		void SetContainsIndirectCalls(bool val) {
			containsIndirectCalls_ = val;
		}

		void SetContainsIndirectJumps(bool val) {
			containsIndirectJumps_ = val;
		}

		void AddPredecessor(uintptr_t addr) {
			predecessors_.push_back(addr);
		}
		
		void AddSuccessor(uintptr_t addr) {
			successors_.push_back(addr);
		}

		std::vector<uintptr_t>& GetSuccessors() {
			return successors_;
		}

		std::vector<uintptr_t>& GetPredecessors() {
			return predecessors_;
		}		

	private:
		BasicBlock(Function* func, const std::string& name, uintptr_t start)
			: func_(func), name_(name), startAddress_(start), endAddress_(0) {
			std::ostringstream oss;
			oss << std::hex << startAddress_;
			name_ += oss.str();
			successors_.reserve(2); // 0 - 2
			predecessors_.reserve(1); // 0 - n
		}

		Function* func_{ nullptr };
		std::string name_;
		uintptr_t startAddress_{ 0 }, endAddress_{ 0 }, lastInsnAddr_{0};
		bool containsIndirectCalls_ = false;
		bool containsIndirectJumps_ = false;

		std::vector<uintptr_t> successors_; // addresses of successor blocks
		std::vector<uintptr_t> predecessors_; // addresses of predecessor blocks
	};

}


#endif
