#ifndef DATA_FLOW_ANALYSIS_PASS_H
#define DATA_FLOW_ANALYSIS_PASS_H

#include <ReWizard/Analysis/PassProvider.h>
#include <Zydis/Zydis.h>
#include <cstdint>
#include <map>

namespace ReWizard {
    class AnalysisContext;
    class Function;
    class BasicBlock;
    class ExtendedInstruction;

    struct RegisterState {
        uintptr_t defAddress = 0;
        uintptr_t knownValue = 0;
        bool isKnown = false;
    };

    using RegisterFile = std::map<ZydisRegister, RegisterState>;

    class DataFlowAnalysisPass : public PassRegistrar<DataFlowAnalysisPass> {
    public:
        explicit DataFlowAnalysisPass()
            : PassRegistrar<DataFlowAnalysisPass>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~DataFlowAnalysisPass() = default;

		bool PreRun(AnalysisContext* context) override;
		bool Run(AnalysisContext* context) override;
		bool PostRun(AnalysisContext* context) override;
		std::string_view Name() const override { return "DataFlowAnalysisPass"; }
		std::vector<std::string_view> Dependencies() const override { return { "StaticControlFlowRebuilder" }; }

    private:
        void AnalyzeFunction(Function* function, AnalysisContext* context);
        RegisterFile AnalyzeBasicBlock(BasicBlock* bb, AnalysisContext* context, const RegisterFile& incoming);
        void ApplyInstruction(ExtendedInstruction* insn, RegisterFile& regs, AnalysisContext* context);
        bool TryResolveIndirectCall(ExtendedInstruction* insn, const RegisterFile& regs, Function* function);
        bool TryResolveIndirectJump(ExtendedInstruction* insn, const RegisterFile& regs, Function* function);
    };
}

#endif
