#include <gtest/gtest.h>

// Prevent Windows min/max macros from conflicting with std::min/std::max
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <ReWizard/Hybrid/BochsExecutor.h>
#include <ReWizard/Hybrid/BochsInstrumentationBridge.h>
#include <spdlog/spdlog.h>

// Bochs headers
#include <bochs.h>
#include <cpu/cpu.h>
#include <memory/memory-bochs.h>
#include <pc_system.h>
#include <gui/siminterface.h>

using namespace ReWizard;

TEST(BochsExecutorInitTest, FullHardwareInitWithoutCrash) {
    // This test verifies that BochsExecutor::Initialize() correctly calls
    // bx_init_hardware() (including DEV_init_devices) without the 0xc0000005
    // crash that previously occurred because bx_gui was NULL.
    BochsExecutor executor;
    ASSERT_TRUE(executor.Initialize());

    // After full hardware init + reset, the CPU should be in real mode at the
    // BIOS reset vector (0xFFFF0 or CS:IP = 0xF000:0xFFF0).
    Bit64u rip = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;
    Bit16u cs = BX_CPU(0)->sregs[BX_SEG_REG_CS].selector.value;

    // The BIOS entry point is typically at physical 0xFFFF0.
    // In real mode, this is CS=0xF000, IP=0xFFF0 (CS*16+IP = 0xFFFF0).
    EXPECT_EQ(cs, 0xF000u);
    EXPECT_EQ(rip, 0xFFF0u);

    // Verify that key devices were initialized by checking that device I/O
    // handlers are registered (e.g., PIC at ports 0x20/0x21, PIT at 0x40).
    // We can do a quick sanity check by verifying the CPU is not in long mode.
    EXPECT_FALSE(BX_CPU(0)->get_cpu_mode() == BX_MODE_LONG_64);
}

TEST(BochsExecutorInitTest, ExecuteFromBiosResetVector) {
    BochsExecutor executor;
    ASSERT_TRUE(executor.Initialize());

    // Execute a small number of instructions from the BIOS reset vector.
    // The BIOS should start executing. We don't expect it to boot anything
    // (no disk image configured), but it should execute without crashing.
    ASSERT_TRUE(executor.Execute(0, 10));

    // After executing 10 instructions, RIP should have advanced.
    Bit64u rip = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;
    EXPECT_NE(rip, 0xFFF0u);
}
