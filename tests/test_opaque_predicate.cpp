#include <gtest/gtest.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Analysis/Passes/OpaquePredicatePass.h>

#include <filesystem>

using namespace ReWizard;

static std::string GetFixturePath() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "test_pe.exe").string();
}

class OpaquePredicateTest : public ::testing::Test {
protected:
    void SetUp() override {
        fixturePath_ = GetFixturePath();
        if (!std::filesystem::exists(fixturePath_)) {
            GTEST_SKIP() << "Fixture test_pe.exe not found at " << fixturePath_;
        }
    }

    std::string fixturePath_;
};

TEST_F(OpaquePredicateTest, RemovesDeadFallthroughWhenAlwaysTaken) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("test_opaque");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0xFFFF0000);
    fnRaw->SetEnd(0xFFFF0020);
    fnRaw->SetHasOpaquePredicates(true);
    fnRaw->GetOpaquePredicateAddresses().insert(0xFFFF0005);
    fnRaw->GetOpaquePredicateResults()[0xFFFF0005] = true; // always taken

    // BB with two successors: branch target and fallthrough
    auto bb = fnRaw->CreateBasicBlock(0xFFFF0000);
    bb->SetEnd(0xFFFF0010);
    bb->AddSuccessor(0xFFFF0100); // branch target
    bb->AddSuccessor(0xFFFF0010); // fallthrough
    fnRaw->AddBasicBlock(std::move(bb));

    module->AddFunction(std::move(fn));

    OpaquePredicatePass pass;
    ASSERT_TRUE(pass.Run(ctx.get()));

    auto& bbs = fnRaw->GetBasicBlocks();
    ASSERT_EQ(bbs.size(), 1);

    auto& succs = bbs[0]->GetSuccessors();
    ASSERT_EQ(succs.size(), 1);
    EXPECT_EQ(succs[0], 0xFFFF0100);
}

TEST_F(OpaquePredicateTest, RemovesDeadBranchTargetWhenAlwaysNotTaken) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("test_opaque2");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0xFFFF0000);
    fnRaw->SetEnd(0xFFFF0020);
    fnRaw->SetHasOpaquePredicates(true);
    fnRaw->GetOpaquePredicateAddresses().insert(0xFFFF0005);
    fnRaw->GetOpaquePredicateResults()[0xFFFF0005] = false; // always not taken

    auto bb = fnRaw->CreateBasicBlock(0xFFFF0000);
    bb->SetEnd(0xFFFF0010);
    bb->AddSuccessor(0xFFFF0100); // branch target
    bb->AddSuccessor(0xFFFF0010); // fallthrough
    fnRaw->AddBasicBlock(std::move(bb));

    module->AddFunction(std::move(fn));

    OpaquePredicatePass pass;
    ASSERT_TRUE(pass.Run(ctx.get()));

    auto& bbs = fnRaw->GetBasicBlocks();
    ASSERT_EQ(bbs.size(), 1);

    auto& succs = bbs[0]->GetSuccessors();
    ASSERT_EQ(succs.size(), 1);
    EXPECT_EQ(succs[0], 0xFFFF0010);
}

TEST_F(OpaquePredicateTest, SkipsFunctionsWithoutOpaquePredicates) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("normal_fn");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0xFFFF0000);
    fnRaw->SetEnd(0xFFFF0010);

    auto bb = fnRaw->CreateBasicBlock(0xFFFF0000);
    bb->SetEnd(0xFFFF0010);
    bb->AddSuccessor(0xFFFF0100);
    bb->AddSuccessor(0xFFFF0010);
    fnRaw->AddBasicBlock(std::move(bb));

    module->AddFunction(std::move(fn));

    OpaquePredicatePass pass;
    ASSERT_TRUE(pass.Run(ctx.get()));

    auto& bbs = fnRaw->GetBasicBlocks();
    ASSERT_EQ(bbs.size(), 1);
    EXPECT_EQ(bbs[0]->GetSuccessors().size(), 2);
}
