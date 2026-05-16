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

namespace ReWizard {

    // Forward declarations from Bochs main.cc
    extern "C" {
        void bx_init_options(void);
        void bx_init_bx_dbg(void);
        extern char *bochsrc_filename;
        extern Bit8u bx_cpu_count;
    }

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

        // Check required binaries
        for (const auto& [name, path] : std::initializer_list<std::pair<const char*, std::filesystem::path>>{
            {"ntoskrnl.exe", ntoskrnlPath},
            {"hal.dll", halPath},
            {"ntdll.dll", ntdllPath},
            {"kernel32.dll", kernel32Path},
            {"kernelbase.dll", kernelbasePath}
        }) {
            if (!std::filesystem::exists(path)) {
                spdlog::error("BochsExecutor: {} not found at {}", name, path.string());
                return false;
            }
        }

        spdlog::info("BochsExecutor: loading Windows system binaries...");

        // Parse PE files using LIEF
        try {
            impl_->ntoskrnl = LIEF::PE::Parser::parse(ntoskrnlPath.string());
            impl_->hal = LIEF::PE::Parser::parse(halPath.string());
            impl_->ntdll = LIEF::PE::Parser::parse(ntdllPath.string());
            impl_->kernel32 = LIEF::PE::Parser::parse(kernel32Path.string());
            impl_->kernelbase = LIEF::PE::Parser::parse(kernelbasePath.string());
        } catch (const std::exception& e) {
            spdlog::error("BochsExecutor: failed to parse PE files: {}", e.what());
            return false;
        }

        spdlog::info("BochsExecutor: parsed PE files successfully");
        spdlog::info("BochsExecutor: ntoskrnl.exe: {} sections, image base 0x{:x}",
                      impl_->ntoskrnl->sections().size(),
                      impl_->ntoskrnl->optional_header().imagebase());

        // Initialize Bochs core (once only)
        if (!g_bochsCoreInitialized) {
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
            if (norcfile != 0) {
                spdlog::error("BochsExecutor: failed to read configuration");
                return false;
            }

            bx_cpu_count = SIM->get_param_num(BXPN_CPU_NPROCESSORS)->get() *
                           SIM->get_param_num(BXPN_CPU_NCORES)->get() *
                           SIM->get_param_num(BXPN_CPU_NTHREADS)->get();
            if (bx_cpu_count == 0) bx_cpu_count = 1;

            // Manual hardware init (skip DEV_init_devices which crashes)
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

            g_bochsCoreInitialized = true;
            spdlog::info("BochsExecutor: Bochs core initialized");
        }

        // Map ntoskrnl.exe into Bochs physical memory
        auto ntoskrnlBase = 0xFFFFF80000000000ULL;
        auto ntoskrnlData = ReadFile(ntoskrnlPath.string());
        if (!ntoskrnlData.empty()) {
            // Write in chunks to avoid cross-page issues in writePhysicalPage
            for (size_t offset = 0; offset < ntoskrnlData.size(); offset += 4096) {
                size_t chunk = std::min<size_t>(4096, ntoskrnlData.size() - offset);
                BX_MEM(0)->writePhysicalPage(BX_CPU(0), ntoskrnlBase + offset, (unsigned)chunk, ntoskrnlData.data() + offset);
            }
            spdlog::info("BochsExecutor: mapped ntoskrnl.exe at 0x{:x} ({} bytes)", ntoskrnlBase, ntoskrnlData.size());
        }

        // Set CPU state for kernel mode execution
        BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx = ntoskrnlBase +
            impl_->ntoskrnl->optional_header().addressof_entrypoint();
        BX_CPU(0)->gen_reg[BX_64BIT_REG_RSP].rrx = ntoskrnlBase + 0x200000;
        BX_CPU(0)->gen_reg[BX_64BIT_REG_RBP].rrx = ntoskrnlBase + 0x200000;
        BX_CPU(0)->cr0.val32 = 0x80050033;
        BX_CPU(0)->cr4.val32 = 0x000006F8;
        BX_CPU(0)->cr3 = 0x00000000001AD000;
        BX_CPU(0)->eflags = 0x0000000000000002;

        spdlog::info("BochsExecutor: CPU state set");
        spdlog::info("BochsExecutor: RIP = 0x{:x}", BX_CPU(0)->gen_reg[BX_64BIT_REG_RIP].rrx);
        spdlog::info("BochsExecutor: RSP = 0x{:x}", BX_CPU(0)->gen_reg[BX_64BIT_REG_RSP].rrx);

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

}
