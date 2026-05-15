#ifndef UNITS_FUNCTION_H
#define UNITS_FUNCTION_H

#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Disassembler/Disassembler.h>
#include <cstdint>
#include <map>
#include <sstream>
#include <iomanip>
#include <memory>
#include <set>
#include <functional>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

namespace ReWizard {

	using InstructionCollection = std::map<uintptr_t, std::unique_ptr<ExtendedInstruction>>;
	using InstructionCollectionIt = InstructionCollection::iterator;
	using PtrBasicBlock = std::unique_ptr<BasicBlock>;
	using BasicBlockCollection = std::vector<PtrBasicBlock>;

	using CallSite = std::pair<uintptr_t, uintptr_t>; // <call address, target address>

	struct BBVertex { BasicBlock* bb; };
	using CFGGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, BBVertex>;
	using VertexMap = std::unordered_map<BasicBlock*, CFGGraph::vertex_descriptor>;

	class Module;

	class Function {
	public:
		using PtrFunction = std::unique_ptr<Function>;
		~Function() = default;

		static PtrFunction Create(Module* module, const std::string& name = "") {
			return std::unique_ptr<Function>(new Function(module, name));
		}

		PtrBasicBlock CreateBasicBlock(uintptr_t start, const std::string& name = "loc_") {
			return BasicBlock::Create(this, start, name);
		}

		BasicBlock* AddBasicBlock(PtrBasicBlock bb) {
			basicBlocks_.push_back(std::move(bb));
			return basicBlocks_.back().get();
		}

		BasicBlockCollection &GetBasicBlocks(){
			return basicBlocks_;
		}

		BasicBlock* GetBasicBlockForAddress(uintptr_t address, bool* wasSplit=nullptr, bool allowSplit=true);

		void NormalizeBasicBlocks();

		const std::vector<std::string>& GetDisassembly(Disassembler& disas, bool forceRecompute=false);

		std::vector<DecodedInstruction*> GetIndirectInstructions();

		uintptr_t GetStart() const {
			return startAddress_;
		}

		uintptr_t GetEnd() const {
			return endAddress_;
		}

		void SetStart(uintptr_t start) {
			startAddress_ = start;
			if (name_.empty()) {
				std::ostringstream oss;
				oss << "sub_" << std::hex << startAddress_;
				name_ = oss.str();
			}
		}

		void SetEnd(uintptr_t end) {
			endAddress_ = end;
		}
		
		uintptr_t GetLastInsnAddr() const { return lastInsnAddr_; }

		void SetLastInsnAddr(uintptr_t addr) {
			lastInsnAddr_ = addr;
		}

		const std::string& GetName() const {
			return name_;
		}

		void SetName(const std::string& name) {
			name_ = name;
		}

		Module* GetModule() const {
			return module_;
		}

		std::set<CallSite>& GetCallSites() {
			return callSites_;
		}

		void AddCallSite(uintptr_t callAddr, uintptr_t targetAddr) {
			auto it = callSites_.find({ callAddr, 0 });
			if (it != callSites_.end()) {
				callSites_.erase(it);
			}
			callSites_.emplace(callAddr, targetAddr);
		}


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

		bool IsMarked() { 
			return markedForHybridAnalysis_; 
		}

		void MarkForHybridAnalysis()  {
			markedForHybridAnalysis_ = true;
		}

		bool HasOpaquePredicates() const {
			return hasOpaquePredicates_;
		}

		void SetHasOpaquePredicates(bool val) {
			hasOpaquePredicates_ = val;
		}

		std::set<uintptr_t>& GetOpaquePredicateAddresses() {
			return opaquePredicateAddresses_;
		}

		bool IsTrampoline() const {
			return isTrampoline_;
		}

		void SetIsTrampoline(bool val) {
			isTrampoline_ = val;
		}

		void AddInstruction(uintptr_t insn) {
			instructions_.insert(insn);
		}

		std::set<uintptr_t>& GetInstructions() {
			return instructions_;
		}

		void BuildCFG();
		std::string DumpCFGToGraphVizString();

		void BasicBlockForEach(const std::function<void(BasicBlock*)>& fn);

	private:
		Function(Module* module, const std::string& name)
			: module_(module), name_(name), startAddress_(0), endAddress_(0) {
		}

		Module* module_{ nullptr };
		BasicBlockCollection basicBlocks_;
		CFGGraph cfgGraph_;
		VertexMap bbVertexMap_;
		uintptr_t startAddress_{ 0 }, endAddress_{ 0 }, lastInsnAddr_{ 0 };
		std::string name_;
		std::set<uintptr_t> instructions_;
		std::set<CallSite> callSites_;
		std::vector<std::string> disassemblyCache_;
		bool containsIndirectCalls_{ false }, containsIndirectJumps_{ false };
		bool markedForHybridAnalysis_ = false;
		bool hasOpaquePredicates_ = false;
		bool disassemblyCached_ = false;
		bool isTrampoline_ = false;
		std::set<uintptr_t> opaquePredicateAddresses_;


	};

}

#endif
