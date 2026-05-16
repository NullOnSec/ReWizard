#include <gtest/gtest.h>
#include <ReWizard/Database/AnalysisDatabase.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <filesystem>
#include <fstream>

using namespace ReWizard;

class AnalysisDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_dbPath = (std::filesystem::temp_directory_path() / "rewizard_test.db").string();
        std::filesystem::remove(m_dbPath);
    }

    void TearDown() override {
        std::filesystem::remove(m_dbPath);
    }

    std::string m_dbPath;
};

TEST_F(AnalysisDatabaseTest, OpenAndCreateSchema) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(db->IsOpen());
    EXPECT_TRUE(db->CreateSchema());
}

TEST_F(AnalysisDatabaseTest, SaveAndQueryFunction) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->CreateSchema());

    auto ctx = AnalysisContext::Create("nonexistent.exe");
    auto module = Module::Create(ctx.get());
    auto fn = module->CreateFunction("main");
    fn->SetStart(0x401000);
    fn->SetEnd(0x401100);
    fn->SetHybridVerified(true);
    fn->SetHasOpaquePredicates(false);
    fn->SetIsTrampoline(false);

    EXPECT_TRUE(db->SaveFunction(*fn));

    auto queried = db->QueryFunctionByAddress(0x401000);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->name, "main");
    EXPECT_EQ(queried->address, 0x401000);
    EXPECT_EQ(queried->size, 0x100);
    EXPECT_TRUE(queried->isHybridVerified);
    EXPECT_FALSE(queried->isTrampoline);

    auto all = db->QueryFunctions();
    EXPECT_EQ(all.size(), 1);
}

TEST_F(AnalysisDatabaseTest, SaveMultipleFunctions) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->CreateSchema());

    auto ctx = AnalysisContext::Create("nonexistent.exe");
    auto module = Module::Create(ctx.get());

    auto fn1 = module->CreateFunction("main");
    fn1->SetStart(0x401000);
    fn1->SetEnd(0x401100);
    module->AddFunction(std::move(fn1));

    auto fn2 = module->CreateFunction("sub_402000");
    fn2->SetStart(0x402000);
    fn2->SetEnd(0x402050);
    module->AddFunction(std::move(fn2));

    EXPECT_TRUE(db->SaveFunctions(module->GetFunctions()));

    auto all = db->QueryFunctions();
    EXPECT_EQ(all.size(), 2);
    EXPECT_EQ(all[0].address, 0x401000);
    EXPECT_EQ(all[1].address, 0x402000);
}

TEST_F(AnalysisDatabaseTest, FunctionUpsert) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->CreateSchema());

    auto ctx = AnalysisContext::Create("nonexistent.exe");
    auto module = Module::Create(ctx.get());

    auto fn = module->CreateFunction("old_name");
    fn->SetStart(0x401000);
    fn->SetEnd(0x401100);
    module->AddFunction(std::move(fn));

    EXPECT_TRUE(db->SaveFunctions(module->GetFunctions()));

    // Update the function name
    auto fn2 = module->CreateFunction("new_name");
    fn2->SetStart(0x401000);
    fn2->SetEnd(0x401200);
    module->AddFunction(std::move(fn2));

    EXPECT_TRUE(db->SaveFunctions(module->GetFunctions()));

    auto queried = db->QueryFunctionByAddress(0x401000);
    ASSERT_TRUE(queried.has_value());
    EXPECT_EQ(queried->name, "new_name");
    EXPECT_EQ(queried->size, 0x200);

    auto all = db->QueryFunctions();
    EXPECT_EQ(all.size(), 1);
}

TEST_F(AnalysisDatabaseTest, SaveAndQueryXrefs) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->CreateSchema());

    EXPECT_TRUE(db->SaveXref(0x401010, 0x402000, "call"));
    EXPECT_TRUE(db->SaveXref(0x401020, 0x402000, "call"));
    EXPECT_TRUE(db->SaveXref(0x401010, 0x403000, "jump"));

    auto to = db->QueryXrefsTo(0x402000);
    EXPECT_EQ(to.size(), 2);
    EXPECT_EQ(to[0].fromAddr, 0x401010);
    EXPECT_EQ(to[0].type, "call");
    EXPECT_EQ(to[1].fromAddr, 0x401020);

    auto from = db->QueryXrefsFrom(0x401010);
    EXPECT_EQ(from.size(), 2);
}

TEST_F(AnalysisDatabaseTest, SaveSymbolTable) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->CreateSchema());

    SymbolTable st;
    st.AddImport({"MessageBoxA", "user32.dll", 0x180002000, 0x180003000});
    st.AddImport({"CreateFileW", "kernel32.dll", 0x180002008, 0x180003008});
    st.AddExport({"DllMain", 0x180001000, 1});

    EXPECT_TRUE(db->SaveSymbolTable(st));

    auto syms = db->QuerySymbolsByAddress(0x180002000);
    EXPECT_EQ(syms.size(), 1);
    EXPECT_EQ(syms[0].name, "MessageBoxA");
    EXPECT_EQ(syms[0].type, "import");
    EXPECT_EQ(syms[0].sourceDll, "user32.dll");
}

TEST_F(AnalysisDatabaseTest, TransactionRollback) {
    auto db = AnalysisDatabase::Open(m_dbPath);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->CreateSchema());

    auto ctx = AnalysisContext::Create("nonexistent.exe");
    auto module = Module::Create(ctx.get());

    auto fn = module->CreateFunction("rollback_test");
    fn->SetStart(0x401000);
    fn->SetEnd(0x401100);
    module->AddFunction(std::move(fn));

    EXPECT_TRUE(db->BeginTransaction());
    EXPECT_TRUE(db->SaveFunction(*module->GetFunctions()[0]));
    EXPECT_TRUE(db->Rollback());

    auto queried = db->QueryFunctionByAddress(0x401000);
    EXPECT_FALSE(queried.has_value());
}

TEST_F(AnalysisDatabaseTest, CloseAndReopen) {
    {
        auto db = AnalysisDatabase::Open(m_dbPath);
        ASSERT_NE(db, nullptr);
        ASSERT_TRUE(db->CreateSchema());

        auto ctx = AnalysisContext::Create("nonexistent.exe");
        auto module = Module::Create(ctx.get());
        auto fn = module->CreateFunction("persistent");
        fn->SetStart(0x401000);
        fn->SetEnd(0x401100);
        module->AddFunction(std::move(fn));
        db->SaveFunctions(module->GetFunctions());
        db->Close();
        EXPECT_FALSE(db->IsOpen());
    }

    {
        auto db = AnalysisDatabase::Open(m_dbPath);
        ASSERT_NE(db, nullptr);
        EXPECT_TRUE(db->IsOpen());

        auto queried = db->QueryFunctionByAddress(0x401000);
        ASSERT_TRUE(queried.has_value());
        EXPECT_EQ(queried->name, "persistent");
    }
}
