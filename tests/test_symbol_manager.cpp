#include <gtest/gtest.h>
#include <ReWizard/Analysis/Units/SymbolManager.h>
#include <ReWizard/Database/AnalysisDatabase.h>
#include <filesystem>

using namespace ReWizard;

class SymbolManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto tempDir = std::filesystem::temp_directory_path();
        dbPath_ = (tempDir / "rewizard_test_symbol_manager.db").string();
        std::filesystem::remove(dbPath_);
        db_ = AnalysisDatabase::Open(dbPath_);
        ASSERT_NE(db_, nullptr);
        ASSERT_TRUE(db_->CreateSchema());
    }

    void TearDown() override {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::string dbPath_;
    std::unique_ptr<AnalysisDatabase> db_;
};

TEST_F(SymbolManagerTest, SetAndGetName) {
    SymbolManager mgr;
    mgr.SetName(0x140001000, "main", "function");
    auto name = mgr.GetName(0x140001000);
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "main");
    auto kind = mgr.GetKind(0x140001000);
    ASSERT_TRUE(kind.has_value());
    EXPECT_EQ(*kind, "function");
}

TEST_F(SymbolManagerTest, SetAndGetComment) {
    SymbolManager mgr;
    mgr.SetComment(0x140001000, "Entry point of the application");
    auto comment = mgr.GetComment(0x140001000);
    ASSERT_TRUE(comment.has_value());
    EXPECT_EQ(*comment, "Entry point of the application");
}

TEST_F(SymbolManagerTest, SetAndGetType) {
    SymbolManager mgr;
    mgr.SetType(0x140001000, "int (*)(int, char**)");
    auto type = mgr.GetType(0x140001000);
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, "int (*)(int, char**)");
}

TEST_F(SymbolManagerTest, GetMissingReturnsNullopt) {
    SymbolManager mgr;
    EXPECT_FALSE(mgr.GetName(0xdeadbeef).has_value());
    EXPECT_FALSE(mgr.GetComment(0xdeadbeef).has_value());
    EXPECT_FALSE(mgr.GetType(0xdeadbeef).has_value());
    EXPECT_FALSE(mgr.GetKind(0xdeadbeef).has_value());
    EXPECT_EQ(mgr.GetAnnotation(0xdeadbeef), nullptr);
}

TEST_F(SymbolManagerTest, RemoveAnnotation) {
    SymbolManager mgr;
    mgr.SetName(0x140001000, "foo");
    EXPECT_TRUE(mgr.GetName(0x140001000).has_value());
    mgr.Remove(0x140001000);
    EXPECT_FALSE(mgr.GetName(0x140001000).has_value());
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST_F(SymbolManagerTest, ClearAll) {
    SymbolManager mgr;
    mgr.SetName(0x140001000, "a");
    mgr.SetName(0x140002000, "b");
    EXPECT_EQ(mgr.Count(), 2u);
    mgr.Clear();
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST_F(SymbolManagerTest, SaveAndLoadRoundTrip) {
    SymbolManager mgr;
    mgr.SetName(0x140001000, "main", "function");
    mgr.SetComment(0x140001000, "Entry point");
    mgr.SetType(0x140001000, "int main(int, char**)");

    mgr.SetName(0x140002000, "g_data", "data");
    mgr.SetType(0x140002000, "uint64_t");

    mgr.SaveTo(db_.get());

    SymbolManager loaded;
    loaded.LoadFrom(db_.get());

    EXPECT_EQ(loaded.Count(), 2u);

    auto a = loaded.GetAnnotation(0x140001000);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name, "main");
    EXPECT_EQ(a->kind, "function");
    EXPECT_EQ(a->comment, "Entry point");
    EXPECT_EQ(a->type, "int main(int, char**)");

    auto b = loaded.GetAnnotation(0x140002000);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->name, "g_data");
    EXPECT_EQ(b->kind, "data");
    EXPECT_EQ(b->type, "uint64_t");
    EXPECT_TRUE(b->comment.empty());
}

TEST_F(SymbolManagerTest, LoadEmptyDatabase) {
    SymbolManager mgr;
    mgr.LoadFrom(db_.get());
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST_F(SymbolManagerTest, UpdateExistingAnnotation) {
    SymbolManager mgr;
    mgr.SetName(0x140001000, "old_name");
    mgr.SetComment(0x140001000, "old comment");
    mgr.SaveTo(db_.get());

    mgr.SetName(0x140001000, "new_name");
    mgr.SetComment(0x140001000, "new comment");
    mgr.SaveTo(db_.get());

    SymbolManager loaded;
    loaded.LoadFrom(db_.get());
    auto a = loaded.GetAnnotation(0x140001000);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name, "new_name");
    EXPECT_EQ(a->comment, "new comment");
}
