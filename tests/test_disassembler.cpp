#include <gtest/gtest.h>
#include <ReWizard/Disassembler/Disassembler.h>

using namespace ReWizard;

class DisassemblerTest : public ::testing::Test {
protected:
    void SetUp() override {
        dis_ = &Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    }

    Disassembler* dis_ = nullptr;
};

TEST_F(DisassemblerTest, DecodeNop) {
    uint8_t data[] = { 0x90 };
    auto insn = (*dis_)->DisassembleSingle<DecodedInstruction>(data, sizeof(data));
    ASSERT_NE(insn, nullptr);
    EXPECT_EQ(insn->Instruction().mnemonic, ZYDIS_MNEMONIC_NOP);
    EXPECT_EQ(insn->Instruction().length, 1);
}

TEST_F(DisassemblerTest, DecodePushRbp) {
    uint8_t data[] = { 0x55 };
    auto insn = (*dis_)->DisassembleSingle<DecodedInstruction>(data, sizeof(data));
    ASSERT_NE(insn, nullptr);
    EXPECT_EQ(insn->Instruction().mnemonic, ZYDIS_MNEMONIC_PUSH);
    EXPECT_EQ(insn->Operands()[0].type, ZYDIS_OPERAND_TYPE_REGISTER);
    EXPECT_EQ(insn->Operands()[0].reg.value, ZYDIS_REGISTER_RBP);
}

TEST_F(DisassemblerTest, DecodeMovImm32) {
    // mov eax, 0x12345678
    uint8_t data[] = { 0xB8, 0x78, 0x56, 0x34, 0x12 };
    auto insn = (*dis_)->DisassembleSingle<DecodedInstruction>(data, sizeof(data));
    ASSERT_NE(insn, nullptr);
    EXPECT_EQ(insn->Instruction().mnemonic, ZYDIS_MNEMONIC_MOV);
    EXPECT_EQ(insn->Instruction().length, 5);
}

TEST_F(DisassemblerTest, DecodeInvalidReturnsNull) {
    uint8_t data[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    auto insn = (*dis_)->DisassembleSingle<DecodedInstruction>(data, sizeof(data));
    EXPECT_EQ(insn, nullptr);
}

TEST_F(DisassemblerTest, InstructionToString) {
    uint8_t data[] = { 0x90 };
    auto insn = (*dis_)->DisassembleSingle<DecodedInstruction>(data, sizeof(data));
    ASSERT_NE(insn, nullptr);
    auto str = (*dis_)->InstructionToString(insn.get(), 0x1000);
    EXPECT_NE(str.find("nop"), std::string::npos);
}

TEST_F(DisassemblerTest, ExtendedInstructionHasIndirectFlag) {
    uint8_t data[] = { 0x90 };
    auto insn = (*dis_)->DisassembleSingle<ExtendedInstruction>(data, sizeof(data));
    ASSERT_NE(insn, nullptr);
    EXPECT_FALSE(insn->IsIndirect());
}
