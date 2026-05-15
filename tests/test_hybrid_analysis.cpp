#include <gtest/gtest.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Passes/HybridAnalysisPass.h>
#include <ReWizard/Analysis/Passes/ImportAnalysisPass.h>
#include <ReWizard/Hybrid/ITraceReader.h>
#include <ReWizard/Hybrid/SimpleTraceReader.h>
#include <ReWizard/Hybrid/TraceRecord.h>
#include <ReWizard/Disassembler/Disassembler.h>

#include <filesystem>
#include <fstream>

using namespace ReWizard;

// Mock trace reader for unit testing
class MockTraceReader : public ITraceReader {
public:
    bool Load(const std::string&) override { return true; }
    const std::vector<TraceRecord>& GetRecords() const override { return records; }
    std::vector<const TraceRecord*> GetRecordsForPC(uintptr_t pc) const override {
        std::vector<const TraceRecord*> result;
        for (const auto& r : records) {
            if (r.pc == pc) result.push_back(&r);
        }
        return result;
    }

    std::vector<TraceRecord> records;
};

static std::string GetFixturePath() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "test_pe.exe").string();
}

class HybridAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override {
        fixturePath_ = GetFixturePath();
        if (!std::filesystem::exists(fixturePath_)) {
            GTEST_SKIP() << "Fixture test_pe.exe not found at " << fixturePath_;
        }
    }

    std::string fixturePath_;
};

TEST_F(HybridAnalysisTest, ResolvesIndirectCallFromTrace) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    // Create a fake function with an indirect call instruction
    // Use high addresses to avoid conflicting with the real PE instructions
    auto fn = module->CreateFunction("test_fn");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0xFFFF0000);
    fnRaw->SetEnd(0xFFFF0010);
    fnRaw->MarkForHybridAnalysis();

    auto bb = fnRaw->CreateBasicBlock(0xFFFF0000);
    bb->SetEnd(0xFFFF0010);
    fnRaw->AddBasicBlock(std::move(bb));

    // Create a fake indirect call instruction at 0xFFFF0000
    {
        auto& disas = Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        auto proxy = disas.operator->();

        // Encode: call rax (FF D0)
        uint8_t callRax[] = { 0xFF, 0xD0 };
        auto insn = proxy->DisassembleSingle<ExtendedInstruction>(callRax, sizeof(callRax));
        ASSERT_NE(insn, nullptr);
        insn->Address() = 0xFFFF0000;
        insn->IsIndirect() = true;
        module->AddInstruction(insn);
    }
    module->AddFunction(std::move(fn));

    // Set up mock trace: at PC 0xFFFF0000, RAX = 0x12345678
    auto mockReader = std::make_unique<MockTraceReader>();
    TraceRecord rec;
    rec.pc = 0xFFFF0000;
    rec.registers[ZYDIS_REGISTER_RAX] = 0x12345678;
    mockReader->records.push_back(std::move(rec));

    HybridAnalysisPass pass;
    pass.SetTraceReader(std::move(mockReader));
    ASSERT_TRUE(pass.PreRun(ctx.get()));
    ASSERT_TRUE(pass.Run(ctx.get()));
    ASSERT_TRUE(pass.PostRun(ctx.get()));

    EXPECT_TRUE(fnRaw->IsHybridVerified());

    bool foundResolvedCall = false;
    for (const auto& cs : fnRaw->GetCallSites()) {
        if (cs.first == 0xFFFF0000 && cs.second == 0x12345678) {
            foundResolvedCall = true;
            break;
        }
    }
    EXPECT_TRUE(foundResolvedCall);
}

TEST_F(HybridAnalysisTest, ClearsOpaquePredicateWhenTraceShowsBothPaths) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("test_fn2");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0xFFFF1000);
    fnRaw->SetEnd(0xFFFF1005);
    fnRaw->MarkForHybridAnalysis();
    fnRaw->SetHasOpaquePredicates(true);
    fnRaw->GetOpaquePredicateAddresses().insert(0xFFFF1002);

    auto bb = fnRaw->CreateBasicBlock(0xFFFF1000);
    bb->SetEnd(0xFFFF1005);
    fnRaw->AddBasicBlock(std::move(bb));

    module->AddFunction(std::move(fn));

    // Mock trace shows the opaque predicate address executed multiple times
    auto mockReader = std::make_unique<MockTraceReader>();
    TraceRecord rec1;
    rec1.pc = 0xFFFF1002;
    mockReader->records.push_back(std::move(rec1));
    TraceRecord rec2;
    rec2.pc = 0xFFFF1002;
    mockReader->records.push_back(std::move(rec2));

    HybridAnalysisPass pass;
    pass.SetTraceReader(std::move(mockReader));
    ASSERT_TRUE(pass.Run(ctx.get()));

    EXPECT_FALSE(fnRaw->HasOpaquePredicates());
    EXPECT_TRUE(fnRaw->GetOpaquePredicateAddresses().empty());
}

TEST_F(HybridAnalysisTest, SkipsWhenNoTracePathSetAndNoReaderInjected) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("test_fn3");
    auto* fnRaw = fn.get();
    fnRaw->MarkForHybridAnalysis();
    module->AddFunction(std::move(fn));

    HybridAnalysisPass pass;
    ASSERT_TRUE(pass.Run(ctx.get()));

    // Should not crash and should return true (skip gracefully)
    EXPECT_FALSE(fnRaw->IsHybridVerified());
}

TEST_F(HybridAnalysisTest, SimpleTraceReaderLoadsJson) {
    auto tempPath = std::filesystem::temp_directory_path() / "rewizard_test_trace.json";
    {
        std::ofstream ofs(tempPath);
        ofs << R"([{"pc": "0x1000", "registers": {"RAX": "0xdeadbeef"}}])";
    }

    SimpleTraceReader reader;
    ASSERT_TRUE(reader.Load(tempPath.string()));
    auto records = reader.GetRecordsForPC(0x1000);
    ASSERT_EQ(records.size(), 1);
    EXPECT_EQ(records[0]->registers.at(ZYDIS_REGISTER_RAX), 0xdeadbeef);

    std::filesystem::remove(tempPath);
}

TEST_F(HybridAnalysisTest, ResolvedTargetTriggersReAnalysis) {
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    // Populate symbol table so we know an export address
    ImportAnalysisPass importPass;
    ASSERT_TRUE(importPass.Run(ctx.get()));

    auto* symTable = module->GetSymbolTable();
    ASSERT_NE(symTable, nullptr);
    const auto* exp = symTable->GetExportByName("TestExport");
    if (!exp) {
        GTEST_SKIP() << "TestExport not found in fixture";
    }
    uintptr_t exportAddr = exp->address;
    ASSERT_NE(exportAddr, 0);

    // Create a fake function with an indirect call
    auto fn = module->CreateFunction("caller_fn");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0xFFFF0000);
    fnRaw->SetEnd(0xFFFF0010);
    fnRaw->MarkForHybridAnalysis();

    auto bb = fnRaw->CreateBasicBlock(0xFFFF0000);
    bb->SetEnd(0xFFFF0010);
    fnRaw->AddBasicBlock(std::move(bb));

    {
        auto& disas = Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        auto proxy = disas.operator->();

        uint8_t callRax[] = { 0xFF, 0xD0 };
        auto insn = proxy->DisassembleSingle<ExtendedInstruction>(callRax, sizeof(callRax));
        ASSERT_NE(insn, nullptr);
        insn->Address() = 0xFFFF0000;
        insn->IsIndirect() = true;
        module->AddInstruction(insn);
    }
    module->AddFunction(std::move(fn));

    // Mock trace resolves the indirect call to TestExport
    auto mockReader = std::make_unique<MockTraceReader>();
    TraceRecord rec;
    rec.pc = 0xFFFF0000;
    rec.registers[ZYDIS_REGISTER_RAX] = exportAddr;
    mockReader->records.push_back(std::move(rec));

    // Count functions before hybrid pass
    size_t fnCountBefore = module->GetFunctions().size();

    HybridAnalysisPass pass;
    pass.SetTraceReader(std::move(mockReader));
    ASSERT_TRUE(pass.Run(ctx.get()));

    // A new function should have been discovered at the export address
    size_t fnCountAfter = module->GetFunctions().size();
    EXPECT_GT(fnCountAfter, fnCountBefore);

    auto* discoveredFn = module->GetFunctionForAddress(exportAddr);
    EXPECT_NE(discoveredFn, nullptr);
}
