#include <gtest/gtest.h>
#include <ReWizard/IR/Lifter.h>
#include <ReWizard/IR/IRBlock.h>

using namespace ReWizard;

TEST(LifterTest, CreatesModuleAndContext) {
    Lifter lifter;
    EXPECT_NE(lifter.GetContext(), nullptr);
    EXPECT_NE(lifter.GetModule(), nullptr);
}

TEST(LifterTest, LiftBasicBlockReturnsSuccess) {
    Lifter lifter;

    // Simple x86_64 nop sled: nop; nop; nop; nop; nop
    uint8_t bytes[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

    Lifter::Options opts;
    opts.baseAddress = 0x401000;
    opts.is64Bit = true;

    auto result = lifter.LiftBasicBlock(bytes, sizeof(bytes), opts);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.bytesConsumed, sizeof(bytes));
    EXPECT_NE(result.function, nullptr);
    EXPECT_NE(result.entryBlock, nullptr);
}

TEST(LifterTest, LiftMovImmProducesIR) {
    Lifter lifter;

    // mov rax, 0x1234
    uint8_t bytes[] = { 0x48, 0xC7, 0xC0, 0x34, 0x12, 0x00, 0x00 };

    Lifter::Options opts;
    opts.baseAddress = 0x401000;
    opts.is64Bit = true;

    auto result = lifter.LiftBasicBlock(bytes, sizeof(bytes), opts);
    EXPECT_TRUE(result.success);

    auto text = lifter.DumpModule();
    EXPECT_NE(text.find("bb_401000"), std::string::npos);
}

TEST(LifterTest, DumpModuleProducesText) {
    Lifter lifter;
    auto text = lifter.DumpModule();
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("rewizard_lift"), std::string::npos);
}

TEST(IRBlockTest, DefaultConstruction) {
    IRBlock block;
    EXPECT_FALSE(block.IsValid());
    EXPECT_EQ(block.GetNativeAddress(), 0);
}

TEST(IRBlockTest, MoveConstruction) {
    IRBlock block1;
    block1.SetName("test_block");
    block1.SetNativeAddress(0x401000);

    IRBlock block2(std::move(block1));
    EXPECT_EQ(block2.GetName(), "test_block");
    EXPECT_EQ(block2.GetNativeAddress(), 0x401000);
}
