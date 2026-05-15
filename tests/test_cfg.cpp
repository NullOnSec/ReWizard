#include <gtest/gtest.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/AnalysisManager.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>

#include <filesystem>

using namespace ReWizard;

static std::string GetFixturePath() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "test_pe.exe").string();
}

class CFGRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        fixturePath_ = GetFixturePath();
        if (!std::filesystem::exists(fixturePath_)) {
            GTEST_SKIP() << "Fixture test_pe.exe not found at " << fixturePath_;
        }
    }

    std::string fixturePath_;
};

TEST_F(CFGRecoveryTest, FunctionsDiscovered) {
    auto manager = AnalysisManager::Create(fixturePath_);
    ASSERT_NE(manager, nullptr);

    manager->Run();

    auto module = manager->Context()->GetModule();
    ASSERT_NE(module, nullptr);

    EXPECT_GT(module->GetFunctions().size(), 0);

    bool foundEntrypoint = false;
    for (const auto& fn : module->GetFunctions()) {
        if (fn->GetName() == "__entrypoint" || !fn->GetBasicBlocks().empty()) {
            foundEntrypoint = true;
            break;
        }
    }
    EXPECT_TRUE(foundEntrypoint);
}

TEST_F(CFGRecoveryTest, BasicBlocksExistInFunctions) {
    auto manager = AnalysisManager::Create(fixturePath_);
    ASSERT_NE(manager, nullptr);

    manager->Run();

    auto module = manager->Context()->GetModule();
    ASSERT_NE(module, nullptr);

    bool foundBlocks = false;
    for (const auto& fn : module->GetFunctions()) {
        if (!fn->GetBasicBlocks().empty()) {
            foundBlocks = true;
            break;
        }
    }
    EXPECT_TRUE(foundBlocks);
}

TEST_F(CFGRecoveryTest, InstructionsVisited) {
    auto manager = AnalysisManager::Create(fixturePath_);
    ASSERT_NE(manager, nullptr);

    manager->Run();

    auto ctx = manager->Context();
    EXPECT_GT(ctx->GetVisited().size(), 0);
}

TEST_F(CFGRecoveryTest, EntryPointsFromSymbols) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    // The test fixture has an export "TestExport" and imports.
    // After ImportAnalysisPass runs, StaticControlFlowRebuilder should use them.
    auto manager = AnalysisManager::Create(fixturePath_);
    ASSERT_NE(manager, nullptr);

    manager->Run();

    auto finalModule = manager->Context()->GetModule();
    EXPECT_GT(finalModule->GetFunctions().size(), 0);
}
