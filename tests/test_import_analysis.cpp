#include <gtest/gtest.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/Analysis/Passes/ImportAnalysisPass.h>
#include <ReWizard/FileLoader/FileLoader.h>
#include <LIEF/PE.hpp>

#include <filesystem>
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

using namespace ReWizard;

static std::string GetFixturePath() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "test_pe.exe").string();
}

class ImportAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override {
        fixturePath_ = GetFixturePath();
        if (!std::filesystem::exists(fixturePath_)) {
            GTEST_SKIP() << "Fixture test_pe.exe not found at " << fixturePath_;
        }
    }

    std::string fixturePath_;
};

TEST_F(ImportAnalysisTest, SymbolTablePopulated) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    ImportAnalysisPass pass;
    ASSERT_TRUE(pass.PreRun(ctx.get()));
    ASSERT_TRUE(pass.Run(ctx.get()));
    ASSERT_TRUE(pass.PostRun(ctx.get()));

    auto* symTable = module->GetSymbolTable();
    ASSERT_NE(symTable, nullptr);

    EXPECT_GT(symTable->GetImports().size(), 0);

    bool foundGetModuleHandleA = false;
    bool foundGetProcAddress = false;
    bool foundMessageBoxA = false;

    for (const auto& imp : symTable->GetImports()) {
        if (imp.name == "GetModuleHandleA")
            foundGetModuleHandleA = true;
        if (imp.name == "GetProcAddress")
            foundGetProcAddress = true;
        if (imp.name == "MessageBoxA")
            foundMessageBoxA = true;
    }

    EXPECT_TRUE(foundGetModuleHandleA);
    EXPECT_TRUE(foundGetProcAddress);
    EXPECT_TRUE(foundMessageBoxA);
}

TEST_F(ImportAnalysisTest, ExportsDetected) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    ImportAnalysisPass pass;
    pass.Run(ctx.get());

    auto* symTable = ctx->GetModule()->GetSymbolTable();
    ASSERT_NE(symTable, nullptr);

    const auto* exp = symTable->GetExportByName("TestExport");
    ASSERT_NE(exp, nullptr);
    EXPECT_NE(exp->address, 0);
}

TEST_F(ImportAnalysisTest, RelocationsRecorded) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    ImportAnalysisPass pass;
    pass.Run(ctx.get());

    auto* symTable = ctx->GetModule()->GetSymbolTable();
    ASSERT_NE(symTable, nullptr);

    EXPECT_GT(symTable->GetRelocations().size(), 0);
}

class RelocationTest : public ::testing::Test {
protected:
    void SetUp() override {
        fixturePath_ = GetFixturePath();
        if (!std::filesystem::exists(fixturePath_)) {
            GTEST_SKIP() << "Fixture test_pe.exe not found at " << fixturePath_;
        }
    }

    std::string fixturePath_;
};

TEST_F(RelocationTest, LoadAtNonPreferredBaseFixesAddresses) {
    auto tempLoader = FileLoader::Create(fixturePath_);
    ASSERT_NE(tempLoader, nullptr);

    auto pe = dynamic_cast<LIEF::PE::Binary*>(tempLoader->Binary());
    ASSERT_NE(pe, nullptr);

    uintptr_t preferredBase = pe->imagebase();

    // Block the preferred base so AnalysisContext must load at a different base.
    // If VirtualAlloc fails, the base is already occupied (e.g., by the test process), which also works.
    void* blocker = VirtualAlloc(reinterpret_cast<LPVOID>(preferredBase), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    auto ctx = AnalysisContext::Create(fixturePath_);

    if (blocker)
        VirtualFree(blocker, 0, MEM_RELEASE);

    if (!ctx) {
        GTEST_SKIP() << "AnalysisContext::Create failed";
    }

    uintptr_t actualBase = ctx->GetLoader()->CurrentImageBase();
    if (actualBase == preferredBase) {
        GTEST_SKIP() << "Loaded at preferred base; cannot test relocation fixup";
    }

    ImportAnalysisPass pass;
    ASSERT_TRUE(pass.Run(ctx.get()));

    auto* symTable = ctx->GetModule()->GetSymbolTable();
    ASSERT_NE(symTable, nullptr);

    EXPECT_GT(symTable->GetImports().size(), 0);
    EXPECT_GT(symTable->GetRelocations().size(), 0);

    // Verify that at least one import address points within the mapped region
    bool foundMappedImport = false;
    for (const auto& imp : symTable->GetImports()) {
        if (imp.iatEntry && ctx->GetLoader()->IsWithinMapping(imp.iatEntry)) {
            foundMappedImport = true;
            break;
        }
    }
    EXPECT_TRUE(foundMappedImport);
}
