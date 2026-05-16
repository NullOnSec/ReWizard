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

    Bit64u pml4Entries[512] = {};
    Bit64u pdptEntries[512] = {};
    Bit64u pdEntries[512] = {};

    pml4Entries[0] = 0x71003;
    pdptEntries[0] = 0x72003;
    for (int i = 0; i < 256; i++) {
        pdEntries[i] = static_cast<Bit64u>(i) * 0x200000 + 0x83;
    }

    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x70000, 4096,
        reinterpret_cast<Bit8u*>(pml4Entries));
    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x71000, 4096,
        reinterpret_cast<Bit8u*>(pdptEntries));
    BX_MEM(0)->writePhysicalPage(BX_CPU(0), 0x72000, 4096,
        reinterpret_cast<Bit8u*>(pdEntries));

    Bit64u readback[2] = {};
    BX_MEM(0)->readPhysicalPage(BX_CPU(0), 0x71000, 8,
        reinterpret_cast<Bit8u*>(readback));

    EXPECT_EQ(readback[0], 0x72003u);
}