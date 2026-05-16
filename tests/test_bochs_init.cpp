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

#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

// Forward declarations from main.cc (not in any header)
void bx_init_hardware(void);
void bx_init_options(void);
void bx_init_bx_dbg(void);
extern char *bochsrc_filename;

// Globals defined in main.cc
extern Bit8u bx_cpu_count;

TEST(BochsInitTest, ManualCpuInitAndExecuteNop) {
    // Step 1: sim interface
    bx_init_siminterface();
    ASSERT_NE(SIM, nullptr);

    // Step 2: initialize logging (must happen before any BX_INFO calls)
    SAFE_GET_IOFUNC();
    SAFE_GET_GENLOG();

    // Step 3: debugger & plugin stubs
    bx_init_bx_dbg();
    plugin_startup();

    // Step 4: options (parameter tree)
    bx_init_options();

    // Step 5: minimal bochsrc
    auto tempDir = std::filesystem::temp_directory_path();
    auto bochsrcPath = tempDir / "rewizard_minimal.bochsrc";
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
    EXPECT_EQ(norcfile, 0);

    // Step 6: set cpu count
    bx_cpu_count = SIM->get_param_num(BXPN_CPU_NPROCESSORS)->get() *
                   SIM->get_param_num(BXPN_CPU_NCORES)->get() *
                   SIM->get_param_num(BXPN_CPU_NTHREADS)->get();
    if (bx_cpu_count == 0) bx_cpu_count = 1;

    // Step 7: MANUAL hardware init (bypass DEV_init_devices which crashes)
    // We only init CPU + memory + pc_system, skipping all device emulation.
    bx_pc_system.initialize(SIM->get_param_num(BXPN_IPS)->get());

    if (SIM->get_param_string(BXPN_LOG_FILENAME)->getptr()[0] != '-') {
        io->init_log(SIM->get_param_string(BXPN_LOG_FILENAME)->getptr());
    }
    io->set_log_prefix(SIM->get_param_string(BXPN_LOG_PREFIX)->getptr());

    // Memory init
    bx_param_num_c *bxp_memsize = SIM->get_param_num(BXPN_MEM_SIZE);
    Bit64u memSize = bxp_memsize->get64() * BX_CONST64(1024 * 1024);
    bx_param_num_c *bxp_host_memsize = SIM->get_param_num(BXPN_HOST_MEM_SIZE);
    Bit64u hostMemSize = bxp_host_memsize->get64() * BX_CONST64(1024 * 1024);
    if (memSize < hostMemSize) hostMemSize = memSize;
    bx_param_num_c *bxp_memblock_size = SIM->get_param_num(BXPN_MEM_BLOCK_SIZE);
    Bit32u memBlockSize = (Bit32u)(bxp_memblock_size->get64() * 1024);
    BX_MEM(0)->init_memory(memSize, hostMemSize, memBlockSize);

    // CPU init
    BX_CPU(0)->initialize();
    BX_CPU(0)->sanity_checks();
    BX_CPU(0)->register_state();
    BX_INSTR_INITIALIZE(0);

    // Reset CPU
    bx_pc_system.Reset(BX_RESET_HARDWARE);

    // Step 8: Write NOPs at physical 0x10000 and point CPU there
    Bit8u nops[] = { 0x90, 0x90, 0x90, 0x90 };
    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x10000, sizeof(nops), nops);

    // Set CPU to flat real mode at 0x10000
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = 0x10000;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.u.segment.base = 0;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.p = 1;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.segment = 1;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.type = BX_DATA_READ_WRITE_ACCESSED;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].cache.dpl = 0;
    BX_CPU(0)->sregs[BX_SEG_REG_CS].selector.value = 0;

    Bit64u ripBefore = BX_CPU(0)->get_rip();
    EXPECT_EQ(ripBefore, 0x10000u);

    // Step 9: execute 4 NOPs using debugger cpu_loop (checks icount guard)
    bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
    BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + 4;
    BX_CPU(0)->cpu_loop_debugger();

    Bit64u ripAfter = BX_CPU(0)->get_rip();
    EXPECT_EQ(ripAfter, 0x10004u);

    // Cleanup
    free(bochsrc_filename);
    bochsrc_filename = nullptr;
    std::filesystem::remove(bochsrcPath);
}
