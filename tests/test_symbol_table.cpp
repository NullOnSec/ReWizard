#include <gtest/gtest.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>

using namespace ReWizard;

TEST(SymbolTableTest, AddAndGetImportByAddress) {
    SymbolTable table;
    ImportedSymbol imp{"GetProcAddress", "kernel32.dll", 0x18000000, 0x18000000};
    table.AddImport(imp);

    const auto* found = table.GetImportByAddress(0x18000000);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "GetProcAddress");
    EXPECT_EQ(found->dll, "kernel32.dll");
}

TEST(SymbolTableTest, GetImportByName) {
    SymbolTable table;
    table.AddImport({"LoadLibraryA", "kernel32.dll", 0x18000010, 0x18000010});

    const auto* found = table.GetImportByName("LoadLibraryA");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->address, 0x18000010);
}

TEST(SymbolTableTest, AddAndGetExportByAddress) {
    SymbolTable table;
    table.AddExport({"MyExportedFn", 0x40001000, 1});

    const auto* found = table.GetExportByAddress(0x40001000);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "MyExportedFn");
    EXPECT_EQ(found->ordinal, 1);
}

TEST(SymbolTableTest, GetExportByName) {
    SymbolTable table;
    table.AddExport({"DllMain", 0x40002000, 0});

    const auto* found = table.GetExportByName("DllMain");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->address, 0x40002000);
}

TEST(SymbolTableTest, LookupMissingReturnsNull) {
    SymbolTable table;
    EXPECT_EQ(table.GetImportByAddress(0x0), nullptr);
    EXPECT_EQ(table.GetImportByName("NonExistent"), nullptr);
    EXPECT_EQ(table.GetExportByAddress(0x0), nullptr);
    EXPECT_EQ(table.GetExportByName("NonExistent"), nullptr);
}

TEST(SymbolTableTest, ClearEmptiesEverything) {
    SymbolTable table;
    table.AddImport({"A", "a.dll", 0x1, 0x1});
    table.AddExport({"B", 0x2, 2});
    table.AddRelocation({0x3, 10});

    table.Clear();

    EXPECT_EQ(table.GetImports().size(), 0);
    EXPECT_EQ(table.GetExports().size(), 0);
    EXPECT_EQ(table.GetRelocations().size(), 0);
    EXPECT_EQ(table.GetImportByAddress(0x1), nullptr);
}

TEST(SymbolTableTest, MultipleImportsAndExports) {
    SymbolTable table;
    table.AddImport({"A", "a.dll", 0x1000, 0x1000});
    table.AddImport({"B", "b.dll", 0x2000, 0x2000});
    table.AddExport({"C", 0x3000, 1});
    table.AddExport({"D", 0x4000, 2});

    EXPECT_EQ(table.GetImports().size(), 2);
    EXPECT_EQ(table.GetExports().size(), 2);

    EXPECT_EQ(table.GetImportByAddress(0x1000)->name, "A");
    EXPECT_EQ(table.GetImportByAddress(0x2000)->name, "B");
    EXPECT_EQ(table.GetExportByAddress(0x3000)->name, "C");
    EXPECT_EQ(table.GetExportByAddress(0x4000)->name, "D");
}
