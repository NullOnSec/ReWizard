#include <gtest/gtest.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Analysis/AnalysisContext.h>

using namespace ReWizard;

class ModuleTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ModuleTest, FunctionLookupByAddress) {
    auto ctx = AnalysisContext::Create("nonexistent.exe");
    if (!ctx) {
        GTEST_SKIP() << "No test binary available; skipping integration test";
    }

    auto module = Module::Create(ctx.get());
    ASSERT_NE(module, nullptr);

    auto fn1 = module->CreateFunction();
    fn1->SetStart(0x1000);
    fn1->SetEnd(0x1100);
    module->AddFunction(std::move(fn1));

    auto fn2 = module->CreateFunction();
    fn2->SetStart(0x2000);
    fn2->SetEnd(0x2100);
    module->AddFunction(std::move(fn2));

    EXPECT_EQ(module->GetFunctionForAddress(0x1050), module->GetFunctions()[0].get());
    EXPECT_EQ(module->GetFunctionForAddress(0x2050), module->GetFunctions()[1].get());
    EXPECT_EQ(module->GetFunctionForAddress(0x0001), nullptr);
    EXPECT_EQ(module->GetFunctionForAddress(0x5000), nullptr);
}

TEST_F(ModuleTest, AddInstructionDedup) {
    auto ctx = AnalysisContext::Create("nonexistent.exe");
    if (!ctx) {
        GTEST_SKIP() << "No test binary available; skipping integration test";
    }

    auto module = Module::Create(ctx.get());
    ASSERT_NE(module, nullptr);

    auto dis = &Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    uint8_t nop[] = { 0x90 };

    auto insn1 = (*dis)->DisassembleSingle<ExtendedInstruction>(nop, sizeof(nop));
    ASSERT_NE(insn1, nullptr);
    insn1->Address() = 0x1000;

    auto insn2 = (*dis)->DisassembleSingle<ExtendedInstruction>(nop, sizeof(nop));
    ASSERT_NE(insn2, nullptr);
    insn2->Address() = 0x1000;

    module->AddInstruction(insn1);
    EXPECT_EQ(module->GetInstructions().size(), 1);

    module->AddInstruction(insn2);
    EXPECT_EQ(module->GetInstructions().size(), 1);
}
