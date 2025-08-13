#ifndef EMULATOR_H
#define EMULATOR_H

#include <unicorn/unicorn.h>

#include <ReWizard/Disassembler/Disassembler.h>

#include <vector>
#include <memory>

namespace ReWizard {
    constexpr uint32_t UC_PROT_READWRITE = UC_PROT_READ | UC_PROT_WRITE;

    using UnicornRegistry = std::pair<uc_x86_reg, std::string>;

    const std::map<ZydisRegister, UnicornRegistry> gRegisterMap = {
        { ZYDIS_REGISTER_NONE, { UC_X86_REG_INVALID,  "none" } },
        { ZYDIS_REGISTER_RAX,  { UC_X86_REG_RAX,      "RAX"  } },
        { ZYDIS_REGISTER_RCX,  { UC_X86_REG_RCX,      "RCX"  } },
        { ZYDIS_REGISTER_RDX,  { UC_X86_REG_RDX,      "RDX"  } },
        { ZYDIS_REGISTER_RBX,  { UC_X86_REG_RBX,      "RBX"  } },
        { ZYDIS_REGISTER_RSP,  { UC_X86_REG_RSP,      "RSP"  } },
        { ZYDIS_REGISTER_RBP,  { UC_X86_REG_RBP,      "RBP"  } },
        { ZYDIS_REGISTER_RSI,  { UC_X86_REG_RSI,      "RSI"  } },
        { ZYDIS_REGISTER_RDI,  { UC_X86_REG_RDI,      "RDI"  } },
        { ZYDIS_REGISTER_R8,   { UC_X86_REG_R8,       "R8"   } },
        { ZYDIS_REGISTER_R9,   { UC_X86_REG_R9,       "R9"   } },
        { ZYDIS_REGISTER_R10,  { UC_X86_REG_R10,      "R10"  } },
        { ZYDIS_REGISTER_R11,  { UC_X86_REG_R11,      "R11"  } },
        { ZYDIS_REGISTER_R12,  { UC_X86_REG_R12,      "R12"  } },
        { ZYDIS_REGISTER_R13,  { UC_X86_REG_R13,      "R13"  } },
        { ZYDIS_REGISTER_R14,  { UC_X86_REG_R14,      "R14"  } },
        { ZYDIS_REGISTER_R15,  { UC_X86_REG_R15,      "R15"  } },
        { ZYDIS_REGISTER_R8D,  { UC_X86_REG_R8D,      "R8d"  } },
        { ZYDIS_REGISTER_R9D,  { UC_X86_REG_R9D,      "R9d"  } },
        { ZYDIS_REGISTER_R10D, { UC_X86_REG_R10D,     "R10d" } },
        { ZYDIS_REGISTER_R11D, { UC_X86_REG_R11D,     "R11d" } },
        { ZYDIS_REGISTER_R12D, { UC_X86_REG_R12D,     "R12d" } },
        { ZYDIS_REGISTER_R13D, { UC_X86_REG_R13D,     "R13d" } },
        { ZYDIS_REGISTER_R14D, { UC_X86_REG_R14D,     "R14d" } },
        { ZYDIS_REGISTER_R15D, { UC_X86_REG_R15D,     "R15d" } },
        { ZYDIS_REGISTER_EAX,  { UC_X86_REG_EAX,      "EAX"  } },
        { ZYDIS_REGISTER_ECX,  { UC_X86_REG_ECX,      "ECX"  } },
        { ZYDIS_REGISTER_EDX,  { UC_X86_REG_EDX,      "EDX"  } },
        { ZYDIS_REGISTER_EBX,  { UC_X86_REG_EBX,      "EBX"  } },
        { ZYDIS_REGISTER_ESP,  { UC_X86_REG_ESP,      "ESP"  } },
        { ZYDIS_REGISTER_EBP,  { UC_X86_REG_EBP,      "EBP"  } },
        { ZYDIS_REGISTER_ESI,  { UC_X86_REG_ESI,      "ESI"  } },
        { ZYDIS_REGISTER_EDI,  { UC_X86_REG_EDI,      "EDI"  } },
    };

    class Emulator {
    public:
        Emulator(uc_arch arch = UC_ARCH_X86, uc_mode mode = UC_MODE_64);
        ~Emulator();

        /* Engine metadata */
        static unsigned int Version(unsigned int* major, unsigned int* minor);
        static bool IsArchSupported(uc_arch arch);

        /* Error mgmnt */
        static const char* StrError(uc_err code);
        uc_err GetLastError() const;
        uc_err GetEngineErrno() const;

        /* Memory mgmnt */
        uc_err MapMemoryRegion(uintptr_t base, uint8_t* data, size_t dataSize);
        uc_err MapMemoryPtr(uintptr_t base, uint8_t* data, size_t dataSize, uint32_t perms = UC_PROT_ALL);
        uc_err UnmapMemory(uintptr_t base, size_t size);
        uc_err MemProtect(uintptr_t base, size_t size, uint32_t perms);
        uc_err MemRegions(uc_mem_region** regions, uint32_t* count);
        uc_err MemWrite(uintptr_t addr, void* data, size_t size);
        uc_err MemRead(uintptr_t addr, void* data, size_t size);
        uc_err MMIOMap(uint64_t addr, size_t sz, uc_cb_mmio_read_t r, void* rd, uc_cb_mmio_write_t w, void* wd);

        static uc_err Free(void* mem);

        /* Register mgmnt */
        uc_err ReadRegister(int regId, uintptr_t* value);
        uc_err WriteRegister(int regId, uintptr_t value);
        uc_err RegRead2(int regId, void* value, size_t* size);
        uc_err RegWrite2(int regId, const void* value, size_t* size);
        uc_err ReadRegisters(int* regs, void** values, int count);
        uc_err WriteRegisters(int* regs, void** values, int count);
        uc_err RegReadBatch2(int* regs, void** vals, size_t* sizes, int count);
        uc_err RegWriteBatch2(int* regs, const void* const* vals, size_t* sizes, int count);

        /* Emulation mgmnt */
        uc_err Emulate(uintptr_t begin, uintptr_t end, uint64_t timeout, size_t count);
        uc_err StopEmulation();

        /* Hook mgmnt */
        uc_err AddHook(int hookType, void* cb, void* userData, uc_hook* hk = nullptr, uintptr_t begin = 1, uintptr_t end = 0);
        uc_err DelHook(uc_hook hook);

        /* Querying engine opts */
        uc_err Query(int query, size_t* result);
        uc_err Ctl(int ctl_type, ...);

        /* Context mgmnt */
        uc_err AllocContext(uc_context** context);
        uc_err FreeContext(uc_context* context);
        uc_err SaveContext(uc_context* context);
        uc_err RestoreContext(uc_context* context);
        uc_err ContextRegWrite(uc_context* ctx, int regid, const void* value);
        uc_err ContextRegRead(uc_context* ctx, int regid, void* value);
        uc_err ContextRegWrite2(uc_context* ctx, int regid, const void* value, size_t* size);
        uc_err ContextRegRead2(uc_context* ctx, int regid, void* value, size_t* size);
        uc_err ContextRegWriteBatch(uc_context* ctx, int* regs, void* const* vals, int count);
        uc_err ContextRegReadBatch(uc_context* ctx, int* regs, void** vals, int count);
        uc_err ContextRegWriteBatch2(uc_context* ctx, int* regs, const void* const* vals, size_t* sizes, int count);
        uc_err ContextRegReadBatch2(uc_context* ctx, int* regs, void** vals, size_t* sizes, int count);
        uc_err ContextSize(size_t* sz) const;

        // Info
        uc_engine* GetEngine() const;
        uc_arch GetArch() const;
        uc_mode GetMode() const;

    private:
        uc_engine*  m_engine{ nullptr };
        uc_err      m_lastError{ UC_ERR_OK };
        uc_arch     m_arch;
        uc_mode     m_mode;
        std::vector<uc_hook> m_unicornHooks;
    };
}

#endif