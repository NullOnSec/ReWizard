#include <gtest/gtest.h>

// Prevent Windows min/max macros from conflicting with std::min/std::max
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// Bochs headers
#include <bochs.h>
#include <param_names.h>
#include <cpu/cpu.h>
#include <memory/memory-bochs.h>
#include <pc_system.h>
#include <gui/siminterface.h>
#include <plugin.h>
#include <bx_debug/debug.h>

// Bochs osdep.h defines snprintf -> _snprintf which breaks Boost headers
#undef snprintf

#include <ReWizard/Hybrid/IEmulator.h>
#include <ReWizard/Hybrid/TraceRecord.h>
#include <ReWizard/Hybrid/BochsInstrumentationBridge.h>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Passes/HybridAnalysisPass.h>
#include <ReWizard/Disassembler/Disassembler.h>

#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <unordered_map>

using namespace ReWizard;

// Forward declarations from Bochs main.cc
void bx_init_options(void);
void bx_init_bx_dbg(void);
extern char *bochsrc_filename;
extern Bit8u bx_cpu_count;

static std::string GetFixturePath() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "test_pe.exe").string();
}

// TraceCollector implements ITraceProducer and stores records in memory.
class TraceCollector : public ITraceProducer {
public:
    std::vector<TraceRecord> records;

    void OnTraceRecord(const TraceRecord& record) override {
        records.push_back(record);
    }

    void OnExecutionComplete() override {}
};

// VectorTraceReader implements ITraceReader from an in-memory vector.
class VectorTraceReader : public ITraceReader {
public:
    std::vector<TraceRecord> records;
    std::unordered_map<uintptr_t, std::vector<size_t>> pcIndex;

    bool Load(const std::string&) override { return true; }

    const std::vector<TraceRecord>& GetRecords() const override { return records; }

    std::vector<const TraceRecord*> GetRecordsForPC(uintptr_t pc) const override {
        std::vector<const TraceRecord*> result;
        auto it = pcIndex.find(pc);
        if (it != pcIndex.end()) {
            result.reserve(it->second.size());
            for (size_t idx : it->second) {
                result.push_back(&records[idx]);
            }
        }
        return result;
    }

    void AddRecord(TraceRecord rec) {
        pcIndex[rec.pc].push_back(records.size());
        records.push_back(std::move(rec));
    }
};

static void EnsureBochsInitialized() {
    if (SIM != nullptr) {
        // Bochs already initialized by a prior test; just reset CPU state.
        bx_pc_system.Reset(BX_RESET_HARDWARE);
        return;
    }

    bx_init_siminterface();
    ASSERT_NE(SIM, nullptr);

    SAFE_GET_IOFUNC();
    SAFE_GET_GENLOG();
    bx_init_bx_dbg();
    plugin_startup();
    bx_init_options();

    auto tempDir = std::filesystem::temp_directory_path();
    auto bochsrcPath = tempDir / "rewizard_e2e.bochsrc";
    {
        std::ofstream ofs(bochsrcPath);
        ofs << "megs: 32\n";
        ofs << "cpu: model=corei7_haswell_4770, count=1\n";
        ofs << "memory: guest=32, host=32\n";
        ofs << "romimage: file=\"Z:/bochs/bochs-3.0-msvc-src/bochs-3.0/bios/BIOS-bochs-latest\"\n";
        ofs << "vgaromimage: file=\"Z:/bochs/bochs-3.0-msvc-src/bochs-3.0/bios/VGABIOS-lgpl-latest.bin\"\n";
        ofs << "display_library: nogui\n";
    }
    bochsrc_filename = _strdup(bochsrcPath.string().c_str());

    SIM->get_param_enum(BXPN_BOCHS_START)->set(BX_RUN_START);
    int norcfile = bx_read_configuration(bochsrc_filename);
    ASSERT_EQ(norcfile, 0);

    bx_cpu_count = SIM->get_param_num(BXPN_CPU_NPROCESSORS)->get() *
                   SIM->get_param_num(BXPN_CPU_NCORES)->get() *
                   SIM->get_param_num(BXPN_CPU_NTHREADS)->get();
    if (bx_cpu_count == 0) bx_cpu_count = 1;

    bx_pc_system.initialize(SIM->get_param_num(BXPN_IPS)->get());
    if (SIM->get_param_string(BXPN_LOG_FILENAME)->getptr()[0] != '-') {
        io->init_log(SIM->get_param_string(BXPN_LOG_FILENAME)->getptr());
    }
    io->set_log_prefix(SIM->get_param_string(BXPN_LOG_PREFIX)->getptr());

    bx_param_num_c *bxp_memsize = SIM->get_param_num(BXPN_MEM_SIZE);
    Bit64u memSize = bxp_memsize->get64() * BX_CONST64(1024 * 1024);
    bx_param_num_c *bxp_host_memsize = SIM->get_param_num(BXPN_HOST_MEM_SIZE);
    Bit64u hostMemSize = bxp_host_memsize->get64() * BX_CONST64(1024 * 1024);
    if (memSize < hostMemSize) hostMemSize = memSize;
    bx_param_num_c *bxp_memblock_size = SIM->get_param_num(BXPN_MEM_BLOCK_SIZE);
    Bit32u memBlockSize = (Bit32u)(bxp_memblock_size->get64() * 1024);
    BX_MEM(0)->init_memory(memSize, hostMemSize, memBlockSize);

    BX_CPU(0)->initialize();
    BX_CPU(0)->sanity_checks();
    BX_CPU(0)->register_state();
    BX_INSTR_INITIALIZE(0);

    bx_pc_system.Reset(BX_RESET_HARDWARE);
}

class BochsHybridE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        fixturePath_ = GetFixturePath();
        if (!std::filesystem::exists(fixturePath_)) {
            GTEST_SKIP() << "Fixture test_pe.exe not found at " << fixturePath_;
        }
    }

    std::string fixturePath_;
};

TEST_F(BochsHybridE2ETest, BochsTraceResolvesIndirectCall) {
    // Step 1: Initialize Bochs (or reuse existing init).
    EnsureBochsInitialized();

    // Step 2: Write a single nop at physical 0x10000.
    Bit8u nop[] = { 0x90 };
    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x10000, sizeof(nop), nop);

    // Step 3: Set CPU state so that before execution RAX holds our target.
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = 0x10000;
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RAX].rrx = 0x20000;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.u.segment.base = 0;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.p = 1;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.segment = 1;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.type = BX_DATA_READ_WRITE_ACCESSED;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.dpl = 0;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].selector.value = 0;

    // Step 4: Set up trace collection.
    TraceCollector collector;
    ReWizard_SetBochsTraceProducer(&collector);
    ReWizard_SetBochsTraceActive(true);

    // Step 5: Execute exactly one instruction.
    bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
    BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + 1;
    BX_CPU(0)->cpu_loop_debugger();

    // Step 6: Stop tracing.
    ReWizard_SetBochsTraceActive(false);
    ReWizard_SetBochsTraceProducer(nullptr);

    // Step 7: Verify we captured exactly one trace record with the expected state.
    ASSERT_EQ(collector.records.size(), 1u);
    EXPECT_EQ(collector.records[0].pc, 0x10000u);
    auto it = collector.records[0].registers.find(ZYDIS_REGISTER_RAX);
    ASSERT_NE(it, collector.records[0].registers.end());
    EXPECT_EQ(it->second, 0x20000u);

    // Step 8: Build a VectorTraceReader from the collected records.
    auto traceReader = std::make_unique<VectorTraceReader>();
    for (auto& rec : collector.records) {
        traceReader->AddRecord(std::move(rec));
    }

    // Step 9: Load the PE fixture and set up a fake function with an indirect call.
    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);

    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("e2e_indirect_caller");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0x10000);
    fnRaw->SetEnd(0x10002); // Just past the call instruction
    fnRaw->MarkForHybridAnalysis();

    auto bb = fnRaw->CreateBasicBlock(0x10000);
    bb->SetEnd(0x10002);
    fnRaw->AddBasicBlock(std::move(bb));

    // Disassemble call rax (FF D0) and place it at 0x10000 in the module.
    {
        auto& disas = Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        auto proxy = disas.operator->();
        uint8_t callRax[] = { 0xFF, 0xD0 };
        auto insn = proxy->DisassembleSingle<ExtendedInstruction>(callRax, sizeof(callRax));
        ASSERT_NE(insn, nullptr);
        insn->Address() = 0x10000;
        insn->IsIndirect() = true;
        module->AddInstruction(insn);
    }

    module->AddFunction(std::move(fn));

    // Step 10: Run HybridAnalysisPass with the real Bochs-generated trace.
    HybridAnalysisPass pass;
    pass.SetTraceReader(std::move(traceReader));
    ASSERT_TRUE(pass.Run(ctx.get()));

    // Step 11: Verify the indirect call target was resolved.
    EXPECT_TRUE(fnRaw->IsHybridVerified());

    bool foundResolvedCall = false;
    for (const auto& cs : fnRaw->GetCallSites()) {
        if (cs.first == 0x10000 && cs.second == 0x20000) {
            foundResolvedCall = true;
            break;
        }
    }
    EXPECT_TRUE(foundResolvedCall);
}

TEST_F(BochsHybridE2ETest, BochsTraceResolvesMultipleExecutions) {
    // Ensure Bochs is ready.
    EnsureBochsInitialized();

    // Write three nops at 0x10000.
    Bit8u nops[] = { 0x90, 0x90, 0x90 };
    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x10000, sizeof(nops), nops);

    // Set CPU state.
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = 0x10000;
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RAX].rrx = 0x30000;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.u.segment.base = 0;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.p = 1;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.segment = 1;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.type = BX_DATA_READ_WRITE_ACCESSED;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.dpl = 0;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].selector.value = 0;

    TraceCollector collector;
    ReWizard_SetBochsTraceProducer(&collector);
    ReWizard_SetBochsTraceActive(true);

    // Execute three nops.
    bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
    BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + 3;
    BX_CPU(0)->cpu_loop_debugger();

    ReWizard_SetBochsTraceActive(false);
    ReWizard_SetBochsTraceProducer(nullptr);

    // We should have three trace records, all with RAX = 0x30000.
    ASSERT_EQ(collector.records.size(), 3u);
    for (const auto& rec : collector.records) {
        auto it = rec.registers.find(ZYDIS_REGISTER_RAX);
        ASSERT_NE(it, rec.registers.end());
        EXPECT_EQ(it->second, 0x30000u);
    }

    // Feed to hybrid analysis.
    auto traceReader = std::make_unique<VectorTraceReader>();
    for (auto& rec : collector.records) {
        traceReader->AddRecord(std::move(rec));
    }

    auto ctx = AnalysisContext::Create(fixturePath_);
    ASSERT_NE(ctx, nullptr);
    auto module = ctx->GetModule();
    ASSERT_NE(module, nullptr);

    auto fn = module->CreateFunction("e2e_multi_nop");
    auto* fnRaw = fn.get();
    fnRaw->SetStart(0x10000);
    fnRaw->SetEnd(0x10004);
    fnRaw->MarkForHybridAnalysis();

    auto bb = fnRaw->CreateBasicBlock(0x10000);
    bb->SetEnd(0x10004);
    fnRaw->AddBasicBlock(std::move(bb));

    {
        auto& disas = Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        auto proxy = disas.operator->();
        uint8_t callRax[] = { 0xFF, 0xD0 };
        auto insn = proxy->DisassembleSingle<ExtendedInstruction>(callRax, sizeof(callRax));
        ASSERT_NE(insn, nullptr);
        insn->Address() = 0x10001; // Second nop position
        insn->IsIndirect() = true;
        module->AddInstruction(insn);
    }

    // Also add a dummy instruction at 0x10000 so the loop doesn't break early.
    {
        auto& disas = Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        auto proxy = disas.operator->();
        uint8_t nop[] = { 0x90 };
        auto insn = proxy->DisassembleSingle<ExtendedInstruction>(nop, sizeof(nop));
        ASSERT_NE(insn, nullptr);
        insn->Address() = 0x10000;
        module->AddInstruction(insn);
    }
    // Add instruction at 0x10003 so loop reaches end.
    {
        auto& disas = Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        auto proxy = disas.operator->();
        uint8_t nop[] = { 0x90 };
        auto insn = proxy->DisassembleSingle<ExtendedInstruction>(nop, sizeof(nop));
        ASSERT_NE(insn, nullptr);
        insn->Address() = 0x10003;
        module->AddInstruction(insn);
    }

    module->AddFunction(std::move(fn));

    HybridAnalysisPass pass;
    pass.SetTraceReader(std::move(traceReader));
    ASSERT_TRUE(pass.Run(ctx.get()));

    EXPECT_TRUE(fnRaw->IsHybridVerified());

    bool found = false;
    for (const auto& cs : fnRaw->GetCallSites()) {
        if (cs.first == 0x10001 && cs.second == 0x30000) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
