#include <ReWizard/Hybrid/BochsExecutor.h>
#include <ReWizard/Hybrid/BochsInstrumentationBridge.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <vector>

// Bochs headers
#include <bochs.h>
#include <param_names.h>
#include <cpu/cpu.h>
#include <memory/memory-bochs.h>
#include <pc_system.h>
#include <gui/siminterface.h>
#include <plugin.h>
#include <bx_debug/debug.h>

// osdep.h defines read/write macros that conflict with std::fstream
#undef read
#undef write

// LIEF for PE parsing
#include <LIEF/PE.hpp>

// Forward declarations from Bochs main.cc (global namespace)
void bx_init_options(void);
void bx_init_bx_dbg(void);
extern char *bochsrc_filename;
extern Bit8u bx_cpu_count;

// Defined in main.cc; loads the display library plugin (required before DEV_init_devices)
bool load_and_init_display_lib(void);
void bx_init_hardware(void);

namespace ReWizard {

    static bool g_bochsCoreInitialized = false;

    static int ZydisToBochsReg(ZydisRegister reg) {
        switch (reg) {
        case ZYDIS_REGISTER_RAX: return BX_64BIT_REG_RAX;
        case ZYDIS_REGISTER_RCX: return BX_64BIT_REG_RCX;
        case ZYDIS_REGISTER_RDX: return BX_64BIT_REG_RDX;
        case ZYDIS_REGISTER_RBX: return BX_64BIT_REG_RBX;
        case ZYDIS_REGISTER_RSP: return BX_64BIT_REG_RSP;
        case ZYDIS_REGISTER_RBP: return BX_64BIT_REG_RBP;
        case ZYDIS_REGISTER_RSI: return BX_64BIT_REG_RSI;
        case ZYDIS_REGISTER_RDI: return BX_64BIT_REG_RDI;
        case ZYDIS_REGISTER_R8:  return BX_64BIT_REG_R8;
        case ZYDIS_REGISTER_R9:  return BX_64BIT_REG_R9;
        case ZYDIS_REGISTER_R10: return BX_64BIT_REG_R10;
        case ZYDIS_REGISTER_R11: return BX_64BIT_REG_R11;
        case ZYDIS_REGISTER_R12: return BX_64BIT_REG_R12;
        case ZYDIS_REGISTER_R13: return BX_64BIT_REG_R13;
        case ZYDIS_REGISTER_R14: return BX_64BIT_REG_R14;
        case ZYDIS_REGISTER_R15: return BX_64BIT_REG_R15;
        case ZYDIS_REGISTER_RIP: return BX_64BIT_REG_RIP;
        default: return -1;
        }
    }

    class BochsExecutor::Impl {
    public:
        ITraceProducer* traceProducer = nullptr;
        bool initialized = false;
        std::filesystem::path binariesDir;
        std::filesystem::path targetPath;
        std::filesystem::path diskImagePath;

        // Parsed PE images
        std::unique_ptr<LIEF::PE::Binary> ntoskrnl;
        std::unique_ptr<LIEF::PE::Binary> hal;
        std::unique_ptr<LIEF::PE::Binary> ntdll;
        std::unique_ptr<LIEF::PE::Binary> kernel32;
        std::unique_ptr<LIEF::PE::Binary> kernelbase;
    };

    BochsExecutor::BochsExecutor() : impl_(std::make_unique<Impl>()) {
        impl_->binariesDir = std::filesystem::path("Z:/ReWizard/binaries");
    }

    BochsExecutor::~BochsExecutor() {
        if (impl_->initialized) {
            Shutdown();
        }
    }

    static std::vector<uint8_t> ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return {};
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    bool BochsExecutor::Initialize() {
        spdlog::info("BochsExecutor: initializing...");

        auto ntoskrnlPath = impl_->binariesDir / "ntoskrnl.exe";
        auto halPath = impl_->binariesDir / "hal.dll";
        auto ntdllPath = impl_->binariesDir / "ntdll.dll";
        auto kernel32Path = impl_->binariesDir / "kernel32.dll";
        auto kernelbasePath = impl_->binariesDir / "kernelbase.dll";

        auto tryLoadBinary = [](const std::string& name, const std::filesystem::path& path) -> std::unique_ptr<LIEF::PE::Binary> {
            if (!std::filesystem::exists(path)) {
                spdlog::warn("BochsExecutor: {} not found at {} (skipping)", name, path.string());
                return nullptr;
            }
            try {
                auto bin = LIEF::PE::Parser::parse(path.string());
                spdlog::info("BochsExecutor: loaded {} ({} sections, image base 0x{:x})",
                              name, bin->sections().size(), bin->optional_header().imagebase());
                return bin;
            } catch (const std::exception& e) {
                spdlog::warn("BochsExecutor: failed to parse {}: {} (skipping)", name, e.what());
                return nullptr;
            }
        };

        impl_->ntoskrnl = tryLoadBinary("ntoskrnl.exe", ntoskrnlPath);
        impl_->hal = tryLoadBinary("hal.dll", halPath);
        impl_->ntdll = tryLoadBinary("ntdll.dll", ntdllPath);
        impl_->kernel32 = tryLoadBinary("kernel32.dll", kernel32Path);
        impl_->kernelbase = tryLoadBinary("kernelbase.dll", kernelbasePath);

        // Initialize Bochs core (once only)
        if (!g_bochsCoreInitialized) {
            if (SIM != nullptr) {
                // Bochs was already initialized by another component (e.g. test fixture).
                // Reuse the existing instance; just reset CPU state to a known baseline.
                spdlog::info("BochsExecutor: Bochs core already initialized, reusing");
                bx_pc_system.Reset(BX_RESET_HARDWARE);
                g_bochsCoreInitialized = true;
            } else {
                spdlog::info("BochsExecutor: initializing Bochs core...");

                bx_init_siminterface();
                SAFE_GET_IOFUNC();
                SAFE_GET_GENLOG();
                bx_init_bx_dbg();
                plugin_startup();
                bx_init_options();

            auto tempDir = std::filesystem::temp_directory_path();
            auto bochsrcPath = tempDir / "rewizard_minimal.bochsrc";
            {
                std::ofstream ofs(bochsrcPath);
                ofs << "cpu: model=corei7_haswell_4770, count=1, ips=50000000, reset_on_triple_fault=1, ignore_bad_msrs=1\n";
                ofs << "memory: guest=512, host=256\n";
                ofs << "romimage: file=\"Z:/bochs/bochs-3.0-msvc-src/bochs-3.0/bios/BIOS-bochs-latest\", options=fastboot\n";
                ofs << "vgaromimage: file=\"Z:/bochs/bochs-3.0-msvc-src/bochs-3.0/bios/VGABIOS-lgpl-latest.bin\"\n";
                ofs << "display_library: nogui\n";
                ofs << "vga: extension=vbe, update_freq=5, realtime=1\n";
                ofs << "mouse: enabled=0\n";
                ofs << "pci: enabled=1, chipset=i440fx\n";
                ofs << "clock: sync=none, time0=local\n";
                if (!impl_->diskImagePath.empty()) {
                    ofs << "floppya: 1_44=\"" << impl_->diskImagePath.string() << "\", status=inserted\n";
                    ofs << "boot: floppy\n";
                } else {
                    ofs << "ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14\n";
                }
                // If no disk image, omit 'boot:' entirely. Bochs defaults to floppy,
                // and 'boot: none' is explicitly rejected by bx_read_configuration().
                ofs << "log: -\n";
                ofs << "panic: action=report\n";
            }
            bochsrc_filename = _strdup(bochsrcPath.string().c_str());

            SIM->get_param_enum(BXPN_BOCHS_START)->set(BX_RUN_START);
            int norcfile = bx_read_configuration(bochsrc_filename);
            if (norcfile != 0) {
                spdlog::error("BochsExecutor: failed to read configuration");
                return false;
            }

            bx_cpu_count = SIM->get_param_num(BXPN_CPU_NPROCESSORS)->get() *
                           SIM->get_param_num(BXPN_CPU_NCORES)->get() *
                           SIM->get_param_num(BXPN_CPU_NTHREADS)->get();
            if (bx_cpu_count == 0) bx_cpu_count = 1;

            // Load display library plugin BEFORE device init.
            // Without this, DEV_init_devices() crashes because bx_gui is NULL
            // when the VGA device tries to call bx_gui->init().
            if (!load_and_init_display_lib()) {
                spdlog::error("BochsExecutor: failed to load display library");
                return false;
            }

            // Full hardware initialization: memory, CPU, ROMs, devices, reset.
            // This replaces the manual init that skipped DEV_init_devices.
            bx_init_hardware();

            g_bochsCoreInitialized = true;
            spdlog::info("BochsExecutor: Bochs core initialized with full device support");
            }
        }

        // After bx_init_hardware(), the CPU is in real mode at the BIOS entry point.
        // For bare-metal ntoskrnl execution we would need to set up page tables and
        // map the kernel image into the high canonical address range. That requires
        // a full bootloader-like setup (page tables, GDT/IDT, HAL stubs) which is
        // beyond the scope of direct BochsExecutor init.
        //
        // The practical path to a running Windows kernel is:
        //   1. Provide a disk image with Windows installed (or Windows PE)
        //   2. Call BootFromDisk() to let the BIOS bootloader -> winload -> ntoskrnl
        //   3. Use instrumentation to trace kernel execution once booted.
        //
        // For now we leave the CPU at the BIOS reset vector so the user can boot
        // from disk, or manually set up the environment for direct kernel execution.

        impl_->initialized = true;
        spdlog::info("BochsExecutor: initialization complete");
        return true;
    }

    void BochsExecutor::Shutdown() {
        spdlog::info("BochsExecutor: shutting down...");
        impl_->ntoskrnl.reset();
        impl_->hal.reset();
        impl_->ntdll.reset();
        impl_->kernel32.reset();
        impl_->kernelbase.reset();
        impl_->initialized = false;
    }

    bool BochsExecutor::LoadBinary(const uint8_t* data, size_t len, uintptr_t baseAddress) {
        if (!data || len == 0) {
            spdlog::error("BochsExecutor: LoadBinary: invalid data");
            return false;
        }
        if (!g_bochsCoreInitialized) {
            spdlog::error("BochsExecutor: LoadBinary: Bochs core not initialized");
            return false;
        }

        for (size_t offset = 0; offset < len; offset += 4096) {
            size_t chunk = std::min<size_t>(4096, len - offset);
            BX_MEM(0)->writePhysicalPage(BX_CPU(0), baseAddress + offset, (unsigned)chunk, const_cast<uint8_t*>(data + offset));
        }

        spdlog::info("BochsExecutor: loaded binary at 0x{:x} ({} bytes)", baseAddress, len);
        return true;
    }

    bool BochsExecutor::SetRegister(ZydisRegister reg, uintptr_t value) {
        int bochsReg = ZydisToBochsReg(reg);
        if (bochsReg < 0) {
            spdlog::warn("BochsExecutor: unsupported register {}", static_cast<int>(reg));
            return false;
        }
        if (!g_bochsCoreInitialized) {
            spdlog::error("BochsExecutor: SetRegister: Bochs core not initialized");
            return false;
        }
        BX_CPU(0)->gen_reg[bochsReg].rrx = static_cast<Bit64u>(value);
        return true;
    }

    bool BochsExecutor::GetRegister(ZydisRegister reg, uintptr_t& value) {
        int bochsReg = ZydisToBochsReg(reg);
        if (bochsReg < 0) {
            spdlog::warn("BochsExecutor: unsupported register {}", static_cast<int>(reg));
            return false;
        }
        if (!g_bochsCoreInitialized) {
            spdlog::error("BochsExecutor: GetRegister: Bochs core not initialized");
            return false;
        }
        value = static_cast<uintptr_t>(BX_CPU(0)->gen_reg[bochsReg].rrx);
        return true;
    }

    bool BochsExecutor::Snapshot(const std::string& name) {
        (void)name;
        spdlog::warn("BochsExecutor: Snapshot() not yet implemented");
        return false;
    }

    bool BochsExecutor::Restore(const std::string& name) {
        (void)name;
        spdlog::warn("BochsExecutor: Restore() not yet implemented");
        return false;
    }

    bool BochsExecutor::Execute(uintptr_t start, size_t maxInstructions) {
        if (!impl_->initialized) {
            spdlog::error("BochsExecutor: not initialized");
            return false;
        }
        if (!g_bochsCoreInitialized) {
            spdlog::error("BochsExecutor: Execute: Bochs core not initialized");
            return false;
        }

        if (start != 0) {
            BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = static_cast<Bit64u>(start);
        }

        Bit64u currentRip = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;
        spdlog::info("BochsExecutor: executing from 0x{:x} (max {} instructions)",
                      currentRip, maxInstructions);

        if (maxInstructions == 0) {
            spdlog::warn("BochsExecutor: maxInstructions=0, nothing to execute");
            return true;
        }

        bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
        BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + maxInstructions;
        BX_CPU(0)->cpu_loop_debugger();

        Bit64u newRip = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;
        spdlog::info("BochsExecutor: execution paused at 0x{:x} (executed {} instructions)",
                      newRip, newRip == currentRip ? 0 : maxInstructions);
        return true;
    }

    void BochsExecutor::SetTraceProducer(ITraceProducer* producer) {
        impl_->traceProducer = producer;
        ReWizard_SetBochsTraceProducer(producer);
    }

    void BochsExecutor::SetDiskImage(const std::string& path) {
        impl_->diskImagePath = path;
        spdlog::info("BochsExecutor: disk image set to {}", path);
    }

    bool BochsExecutor::BootFromDisk(size_t maxInstructions) {
        if (!impl_->initialized) {
            spdlog::error("BochsExecutor: not initialized");
            return false;
        }
        if (!g_bochsCoreInitialized) {
            spdlog::error("BochsExecutor: BootFromDisk: Bochs core not initialized");
            return false;
        }
        if (impl_->diskImagePath.empty()) {
            spdlog::error("BochsExecutor: BootFromDisk: no disk image configured. Call SetDiskImage() first.");
            return false;
        }

        // After bx_init_hardware(), the CPU is at the BIOS reset vector.
        // We don't touch RIP; just execute instructions and let the BIOS boot.
        Bit64u currentRip = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;
        spdlog::info("BochsExecutor: booting from disk (RIP=0x{:x}, max {} instructions)",
                      currentRip, maxInstructions);

        if (maxInstructions == 0) {
            spdlog::warn("BochsExecutor: maxInstructions=0, nothing to execute");
            return true;
        }

        bx_guard.guard_for |= BX_DBG_GUARD_ICOUNT;
        BX_CPU(0)->guard_found.icount_max = BX_CPU(0)->get_icount() + maxInstructions;
        BX_CPU(0)->cpu_loop_debugger();

        Bit64u newRip = BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx;
        spdlog::info("BochsExecutor: boot paused at 0x{:x}", newRip);
        return true;
    }

}
