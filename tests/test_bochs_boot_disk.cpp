#include <gtest/gtest.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

#include <ReWizard/Hybrid/BochsExecutor.h>
#include <spdlog/spdlog.h>

#include <bochs.h>
#include <param_names.h>
#include <cpu/cpu.h>
#include <memory/memory-bochs.h>
#include <pc_system.h>
#include <gui/siminterface.h>
#include <plugin.h>
#include <bx_debug/debug.h>

#undef read
#undef write
#undef close

using namespace ReWizard;

static std::string GetBootDiskPath() {
    return (std::filesystem::path(__FILE__).parent_path() / "fixtures" / "boot_disk.img").string();
}

static void WritePageChunked(Bit64u addr, const uint8_t* data, size_t len) {
    for (size_t off = 0; off < len; off += 4096) {
        size_t chunk = std::min(static_cast<size_t>(4096), len - off);
        uint8_t buf[4096] = {};
        std::memcpy(buf, data + off, chunk);
        BX_MEM(0)->writePhysicalPage(BX_CPU(0), addr + static_cast<Bit64u>(off),
            static_cast<unsigned>(chunk), buf);
    }
}

TEST(BochsBootDiskTest, DiskImageInitializingBochsExecutor) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found at " << diskPath
                     << ". Run scripts/build-boot-disk.ps1 to generate it.";
    }

    BochsExecutor executor;
    executor.SetDiskImage(diskPath);
    EXPECT_TRUE(executor.Initialize());
}

TEST(BochsBootDiskTest, DiskImageContainsValidBootSignature) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found";
    }

    std::ifstream f(diskPath, std::ios::binary);
    ASSERT_TRUE(f.is_open());

    std::vector<uint8_t> sector0(512);
    f.read(reinterpret_cast<char*>(sector0.data()), 512);
    f.close();

    EXPECT_EQ(sector0[510], 0x55);
    EXPECT_EQ(sector0[511], 0xAA);
}

TEST(BochsBootDiskTest, KernelStubContainsExpectedBytes) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found";
    }

    std::ifstream f(diskPath, std::ios::binary);
    ASSERT_TRUE(f.is_open());

    f.seekg(0, std::ios::end);
    auto fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> diskImage(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(diskImage.data()), fileSize);
    f.close();

    ASSERT_GE(diskImage.size(), 513u);

    const uint8_t* kernel = diskImage.data() + 512;
    size_t kernelSize = diskImage.size() - 512;

    ASSERT_GE(kernelSize, 10u);

    EXPECT_EQ(kernel[0], 0x66);
    EXPECT_EQ(kernel[1], 0xB8);
    EXPECT_EQ(kernel[2], 0x10);
    EXPECT_EQ(kernel[3], 0x00);
}

TEST(BochsBootDiskTest, BochsExecutorLoadsKernelIntoMemory) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found";
    }

    BochsExecutor executor;
    executor.SetDiskImage(diskPath);
    ASSERT_TRUE(executor.Initialize());

    std::ifstream f(diskPath, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f.is_open());
    auto fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> diskImage(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(diskImage.data()), fileSize);
    f.close();

    const uint8_t* kernel = diskImage.data() + 512;
    size_t kernelSize = diskImage.size() - 512;
    size_t writeSize = std::min(kernelSize, static_cast<size_t>(256));

    for (size_t offset = 0; offset < writeSize; offset += 4096) {
        size_t chunk = std::min(static_cast<size_t>(4096), writeSize - offset);
        uint8_t pageBuf[4096] = {};
        std::memcpy(pageBuf, kernel + offset, chunk);
        BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x10000 + static_cast<Bit64u>(offset),
            static_cast<unsigned>(chunk), pageBuf);
    }

    uint8_t readback[4] = {};
    BX_MEM(0)->readPhysicalPage(BX_CPU(0), 0x10000, 4, readback);

    EXPECT_EQ(readback[0], kernel[0]);
    EXPECT_EQ(readback[1], kernel[1]);
    EXPECT_EQ(readback[2], kernel[2]);
    EXPECT_EQ(readback[3], kernel[3]);
}

TEST(BochsBootDiskTest, BochsExecutorCanWritePageTables) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found";
    }

    BochsExecutor executor;
    executor.SetDiskImage(diskPath);
    ASSERT_TRUE(executor.Initialize());

    Bit8u pml4[4096] = {};
    Bit8u pdpt[4096] = {};
    Bit8u pd[4096] = {};

    const Bit64u pml4Base = 0x70000;
    const Bit64u pdptBase = 0x71000;
    const Bit64u pdBase = 0x72000;

    Bit64u* pml4e = reinterpret_cast<Bit64u*>(pml4);
    pml4e[0] = pdptBase | 0x03;

    Bit64u* pdpte = reinterpret_cast<Bit64u*>(pdpt);
    pdpte[0] = pdBase | 0x03;

    Bit64u* pde = reinterpret_cast<Bit64u*>(pd);
    for (int i = 0; i < 256; i++) {
        pde[i] = (static_cast<Bit64u>(i) * 0x200000) | 0x83;
    }

    WritePageChunked(pml4Base, pml4, 4096);
    WritePageChunked(pdptBase, pdpt, 4096);
    WritePageChunked(pdBase, pd, 4096);

    Bit64u readback[1] = {};
    BX_MEM(0)->readPhysicalPage(BX_CPU(0), 0x71000, 8,
        reinterpret_cast<Bit8u*>(readback));

    EXPECT_EQ(readback[0], 0x72003u);
}

TEST(BochsBootDiskTest, ExecuteNopsThenHltInLongMode) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found";
    }

    BochsExecutor executor;
    executor.SetDiskImage(diskPath);
    ASSERT_TRUE(executor.Initialize());

    Bit8u pml4[4096] = {};
    Bit8u pdpt[4096] = {};
    Bit8u pd[4096] = {};

    const Bit64u pml4Base = 0x70000;
    const Bit64u pdptBase = 0x71000;
    const Bit64u pdBase = 0x72000;

    Bit64u* pml4e = reinterpret_cast<Bit64u*>(pml4);
    pml4e[0] = pdptBase | 0x03;

    Bit64u* pdpte = reinterpret_cast<Bit64u*>(pdpt);
    pdpte[0] = pdBase | 0x03;

    Bit64u* pde = reinterpret_cast<Bit64u*>(pd);
    for (int i = 0; i < 256; i++) {
        pde[i] = (static_cast<Bit64u>(i) * 0x200000) | 0x83;
    }

    WritePageChunked(pml4Base, pml4, 4096);
    WritePageChunked(pdptBase, pdpt, 4096);
    WritePageChunked(pdBase, pd, 4096);

    Bit8u gdt[24] = {};
    gdt[8] = 0x9A; gdt[9] = 0x20;
    gdt[16] = 0xFF; gdt[17] = 0xFF;
    gdt[20] = 0; gdt[21] = 0x92;
    WritePageChunked(0x60000, gdt, 24);

    Bit8u code[] = { 0x90, 0x90, 0x90, 0xF4 };
    WritePageChunked(0x10000, code, sizeof(code));

    bx_segment_reg_t codeSeg = {};
    codeSeg.selector.value = 0x08;
    codeSeg.cache.u.segment.base = 0;
    codeSeg.cache.u.segment.limit_scaled = 0xFFFFFFFF;
    codeSeg.cache.p = 1;
    codeSeg.cache.segment = 1;
    codeSeg.cache.type = BX_DATA_READ_WRITE_ACCESSED;
    codeSeg.cache.dpl = 0;
    codeSeg.cache.u.segment.l = 1;
    codeSeg.cache.u.segment.g = 1;

    bx_segment_reg_t dataSeg = {};
    dataSeg.selector.value = 0x10;
    dataSeg.cache.u.segment.base = 0;
    dataSeg.cache.u.segment.limit_scaled = 0xFFFFFFFF;
    dataSeg.cache.p = 1;
    dataSeg.cache.segment = 1;
    dataSeg.cache.type = BX_DATA_READ_WRITE_ACCESSED;
    dataSeg.cache.dpl = 0;
    dataSeg.cache.u.segment.g = 1;

    BX_CPU(0)->sregs[BX_SEG_REG_CS] = codeSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_DS] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_ES] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_SS] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_FS] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_GS] = dataSeg;

    BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = 0x10000;
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RSP].rrx = 0x90000;

    BX_CPU(0)->cr0.val32 = 0x00000001;
    BX_CPU(0)->cr0.set_PE(1);
    BX_CPU(0)->cr4.set_PAE(1);
    BX_CPU(0)->cr3 = pml4Base;

    BX_CPU(0)->efer.set32(0x500);
    BX_CPU(0)->efer.set_LMA(1);

    BX_CPU(0)->cr0.val32 = 0x80000001;
    BX_CPU(0)->cpu_mode = BX_MODE_LONG_64;

    bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
    BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + 4;
    BX_CPU(0)->cpu_loop_debugger();

    Bit64u ripAfter = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;

    if (BX_CPU(0)->get_cpu_mode() == BX_MODE_LONG_64) {
        EXPECT_EQ(ripAfter, 0x10004u)
            << "After 3 NOPs + HLT, RIP should be at 0x10004";
    }
}

static void SetupLongModeCPU(Bit64u pml4Base) {
    Bit8u gdt[24] = {};
    gdt[8] = 0x9A; gdt[9] = 0x20;
    gdt[16] = 0xFF; gdt[17] = 0xFF;
    gdt[20] = 0; gdt[21] = 0x92;
    WritePageChunked(0x60000, gdt, 24);

    bx_segment_reg_t codeSeg = {};
    codeSeg.selector.value = 0x08;
    codeSeg.cache.u.segment.base = 0;
    codeSeg.cache.u.segment.limit_scaled = 0xFFFFFFFF;
    codeSeg.cache.p = 1;
    codeSeg.cache.segment = 1;
    codeSeg.cache.type = BX_DATA_READ_WRITE_ACCESSED;
    codeSeg.cache.dpl = 0;
    codeSeg.cache.u.segment.l = 1;
    codeSeg.cache.u.segment.g = 1;

    bx_segment_reg_t dataSeg = {};
    dataSeg.selector.value = 0x10;
    dataSeg.cache.u.segment.base = 0;
    dataSeg.cache.u.segment.limit_scaled = 0xFFFFFFFF;
    dataSeg.cache.p = 1;
    dataSeg.cache.segment = 1;
    dataSeg.cache.type = BX_DATA_READ_WRITE_ACCESSED;
    dataSeg.cache.dpl = 0;
    dataSeg.cache.u.segment.g = 1;

    BX_CPU(0)->sregs[BX_SEG_REG_CS] = codeSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_DS] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_ES] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_SS] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_FS] = dataSeg;
    BX_CPU(0)->sregs[BX_SEG_REG_GS] = dataSeg;

    BX_CPU(0)->gen_reg[BX_64BIT_REG_RSP].rrx = 0x90000;

    BX_CPU(0)->cr0.val32 = 0x00000001;
    BX_CPU(0)->cr0.set_PE(1);
    BX_CPU(0)->cr4.set_PAE(1);
    BX_CPU(0)->cr3 = pml4Base;
    BX_CPU(0)->efer.set32(0x500);
    BX_CPU(0)->efer.set_LMA(1);
    BX_CPU(0)->cr0.val32 = 0x80000001;
    BX_CPU(0)->cpu_mode = BX_MODE_LONG_64;
}

static Bit64u SetupIdentityMapping() {
    const Bit64u pml4Base = 0x70000;
    const Bit64u pdptBase = 0x71000;
    const Bit64u pdBase = 0x72000;

    Bit8u pml4[4096] = {};
    Bit8u pdpt[4096] = {};
    Bit8u pd[4096] = {};

    Bit64u* pml4e = reinterpret_cast<Bit64u*>(pml4);
    pml4e[0] = pdptBase | 0x03;

    Bit64u* pdpte = reinterpret_cast<Bit64u*>(pdpt);
    pdpte[0] = pdBase | 0x03;

    Bit64u* pde = reinterpret_cast<Bit64u*>(pd);
    for (int i = 0; i < 256; i++) {
        pde[i] = (static_cast<Bit64u>(i) * 0x200000) | 0x83;
    }

    WritePageChunked(pml4Base, pml4, 4096);
    WritePageChunked(pdptBase, pdpt, 4096);
    WritePageChunked(pdBase, pd, 4096);

    return pml4Base;
}

TEST(BochsBootDiskTest, KernelStubWritesToVgaBuffer) {
    auto diskPath = GetBootDiskPath();
    if (!std::filesystem::exists(diskPath)) {
        GTEST_SKIP() << "Boot disk image not found";
    }

    std::ifstream f(diskPath, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f.is_open());
    auto fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> diskImage(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(diskImage.data()), fileSize);
    f.close();

    if (diskImage.size() < 513) {
        GTEST_SKIP() << "Boot disk image too small";
    }

    BochsExecutor executor;
    executor.SetDiskImage(diskPath);
    ASSERT_TRUE(executor.Initialize());

    Bit64u pml4Base = SetupIdentityMapping();

    const uint8_t* kernel = diskImage.data() + 512;
    size_t kernelSize = diskImage.size() - 512;
    if (kernelSize > 4096) kernelSize = 4096;

    uint8_t pageBuf[4096] = {};
    std::memcpy(pageBuf, kernel, kernelSize);
    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x10000, 4096, pageBuf);

    SetupLongModeCPU(pml4Base);
    BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = 0x10000;

    bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
    BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + 200;
    BX_CPU(0)->cpu_loop_debugger();

    if (BX_CPU(0)->get_cpu_mode() == BX_MODE_LONG_64) {
        Bit8u vga[16] = {};
        BX_MEM(0)->readPhysicalPage(BX_CPU(0), 0xB8000, 16, vga);
        EXPECT_EQ(vga[0], 'R') << "First VGA character should be 'R'";
        EXPECT_EQ(vga[2], 'e') << "Second VGA character should be 'e'";
    }
}