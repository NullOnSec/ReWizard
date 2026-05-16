#include <gtest/gtest.h>
#include <ReWizard/Analysis/Units/XrefManager.h>
#include <ReWizard/Database/AnalysisDatabase.h>
#include <filesystem>

using namespace ReWizard;

class XrefManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_dbPath = (std::filesystem::temp_directory_path() / "rewizard_xref_test.db").string();
        std::filesystem::remove(m_dbPath);
    }

    void TearDown() override {
        std::filesystem::remove(m_dbPath);
    }

    std::string m_dbPath;
};

TEST_F(XrefManagerTest, AddAndGetXrefs) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401010, 0x402000, "call");
    mgr.AddXref(0x401020, 0x403000, "jump");
    mgr.AddXref(0x401000, 0x404000, "data");

    auto to = mgr.GetXrefsTo(0x402000);
    EXPECT_EQ(to.size(), 2);
    EXPECT_EQ(to[0].from, 0x401000);
    EXPECT_EQ(to[1].from, 0x401010);

    auto from = mgr.GetXrefsFrom(0x401000);
    EXPECT_EQ(from.size(), 2);
}

TEST_F(XrefManagerTest, GetCallsAndJumps) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401010, 0x403000, "jump");
    mgr.AddXref(0x401020, 0x404000, "call");

    auto calls = mgr.GetCallsTo(0x402000);
    EXPECT_EQ(calls.size(), 1);
    EXPECT_EQ(calls[0].type, "call");

    auto jumps = mgr.GetJumpsFrom(0x401010);
    EXPECT_EQ(jumps.size(), 1);
    EXPECT_EQ(jumps[0].to, 0x403000);

    auto noJumps = mgr.GetJumpsTo(0x402000);
    EXPECT_EQ(noJumps.size(), 0);
}

TEST_F(XrefManagerTest, DuplicateXrefsIgnored) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401000, 0x402000, "call");

    auto xrefs = mgr.GetXrefsTo(0x402000);
    EXPECT_EQ(xrefs.size(), 1);
}

TEST_F(XrefManagerTest, RemoveXref) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401010, 0x402000, "call");
    mgr.RemoveXref(0x401000, 0x402000, "call");

    auto xrefs = mgr.GetXrefsTo(0x402000);
    EXPECT_EQ(xrefs.size(), 1);
    EXPECT_EQ(xrefs[0].from, 0x401010);
}

TEST_F(XrefManagerTest, CountAndHas) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401010, 0x402000, "call");
    mgr.AddXref(0x401000, 0x403000, "jump");

    EXPECT_EQ(mgr.GetXrefCountTo(0x402000), 2);
    EXPECT_EQ(mgr.GetXrefCountFrom(0x401000), 2);
    EXPECT_TRUE(mgr.HasXrefsTo(0x402000));
    EXPECT_TRUE(mgr.HasXrefsFrom(0x401000));
    EXPECT_FALSE(mgr.HasXrefsTo(0x999999));
    EXPECT_FALSE(mgr.HasXrefsFrom(0x999999));
}

TEST_F(XrefManagerTest, Clear) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.Clear();

    EXPECT_EQ(mgr.GetXrefsTo(0x402000).size(), 0);
    EXPECT_EQ(mgr.GetXrefsFrom(0x401000).size(), 0);
}

TEST_F(XrefManagerTest, SaveAndLoad) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x402000, "call");
    mgr.AddXref(0x401010, 0x403000, "jump");
    mgr.AddXref(0x401020, 0x402000, "call");

    {
        auto db = AnalysisDatabase::Open(m_dbPath);
        ASSERT_NE(db, nullptr);
        ASSERT_TRUE(db->CreateSchema());
        mgr.SaveTo(db.get());
    }

    XrefManager loaded;
    {
        auto db = AnalysisDatabase::Open(m_dbPath);
        ASSERT_NE(db, nullptr);
        loaded.LoadFrom(db.get());
    }

    EXPECT_EQ(loaded.GetXrefCountTo(0x402000), 2);
    EXPECT_EQ(loaded.GetXrefCountFrom(0x401010), 1);
    EXPECT_EQ(loaded.GetJumpsFrom(0x401010).size(), 1);
}

TEST_F(XrefManagerTest, DataRefs) {
    XrefManager mgr;
    mgr.AddXref(0x401000, 0x404000, "data");
    mgr.AddXref(0x401010, 0x404000, "data");
    mgr.AddXref(0x401020, 0x404000, "call");

    auto dataRefs = mgr.GetDataRefsTo(0x404000);
    EXPECT_EQ(dataRefs.size(), 2);
    EXPECT_EQ(dataRefs[0].type, "data");
    EXPECT_EQ(dataRefs[1].type, "data");
}
