#include <ReWizard/Emulator/Emulator.h>

namespace ReWizard {
    Emulator::Emulator(uc_arch arch, uc_mode mode)
        : m_arch(arch), m_mode(mode)
    {
        m_lastError = uc_open(arch, mode, &m_engine);
    }

    Emulator::~Emulator() {
        if (m_engine) {
            for (auto& hook : m_unicornHooks) uc_hook_del(m_engine, hook);
            uc_close(m_engine);
        }
    }

    unsigned int Emulator::Version(unsigned int* major, unsigned int* minor) {
        return uc_version(major, minor);
    }
    bool Emulator::IsArchSupported(uc_arch arch) { return uc_arch_supported(arch); }
    uc_err Emulator::GetLastError() const { return m_lastError; }
    uc_err Emulator::GetEngineErrno() const { return uc_errno(m_engine); }
    const char* Emulator::StrError(uc_err code) { return uc_strerror(code); }

    uc_err Emulator::MapMemoryRegion(uintptr_t base, uint8_t* data, size_t dataSize) {
        m_lastError = uc_mem_map(m_engine, base, dataSize, UC_PROT_ALL);
        if (m_lastError != UC_ERR_OK) return m_lastError;
        if (data) m_lastError = uc_mem_write(m_engine, base, data, dataSize);
        return m_lastError;
    }
    uc_err Emulator::MapMemoryPtr(uintptr_t base, uint8_t* data, size_t dataSize, uint32_t perms) {
        m_lastError = uc_mem_map_ptr(m_engine, base, dataSize, perms, data);
        return m_lastError;
    }
    uc_err Emulator::UnmapMemory(uintptr_t base, size_t size) {
        m_lastError = uc_mem_unmap(m_engine, base, size);
        return m_lastError;
    }
    uc_err Emulator::MemProtect(uintptr_t base, size_t size, uint32_t perms) {
        m_lastError = uc_mem_protect(m_engine, base, size, perms);
        return m_lastError;
    }
    uc_err Emulator::MemRegions(uc_mem_region** regions, uint32_t* count) {
        m_lastError = uc_mem_regions(m_engine, regions, count);
        return m_lastError;
    }
    uc_err Emulator::MemWrite(uintptr_t addr, void* data, size_t size) {
        m_lastError = uc_mem_write(m_engine, addr, data, size);
        return m_lastError;
    }
    uc_err Emulator::MemRead(uintptr_t addr, void* data, size_t size) {
        m_lastError = uc_mem_read(m_engine, addr, data, size);
        return m_lastError;
    }
    uc_err Emulator::MMIOMap(uint64_t addr, size_t sz, uc_cb_mmio_read_t r, void* rd, uc_cb_mmio_write_t w, void* wd) {
        m_lastError = uc_mmio_map(m_engine, addr, sz, r, rd, w, wd);
        return m_lastError;
    }

    uc_err Emulator::Free(void* mem) { return uc_free(mem); }

    uc_err Emulator::ReadRegister(int regId, uintptr_t* value) {
        m_lastError = uc_reg_read(m_engine, regId, value);
        return m_lastError;
    }
    uc_err Emulator::WriteRegister(int regId, uintptr_t value) {
        m_lastError = uc_reg_write(m_engine, regId, &value);
        return m_lastError;
    }
    uc_err Emulator::RegRead2(int regId, void* value, size_t* size) {
        m_lastError = uc_reg_read2(m_engine, regId, value, size);
        return m_lastError;
    }
    uc_err Emulator::RegWrite2(int regId, const void* value, size_t* size) {
        m_lastError = uc_reg_write2(m_engine, regId, value, size);
        return m_lastError;
    }
    uc_err Emulator::ReadRegisters(int* regs, void** values, int count) {
        m_lastError = uc_reg_read_batch(m_engine, regs, values, count);
        return m_lastError;
    }
    uc_err Emulator::WriteRegisters(int* regs, void** values, int count) {
        m_lastError = uc_reg_write_batch(m_engine, regs, values, count);
        return m_lastError;
    }
    uc_err Emulator::RegReadBatch2(int* regs, void** vals, size_t* sizes, int count) {
        m_lastError = uc_reg_read_batch2(m_engine, regs, vals, sizes, count);
        return m_lastError;
    }
    uc_err Emulator::RegWriteBatch2(int* regs, const void* const* vals, size_t* sizes, int count) {
        m_lastError = uc_reg_write_batch2(m_engine, regs, vals, sizes, count);
        return m_lastError;
    }

    uc_err Emulator::Emulate(uintptr_t begin, uintptr_t end, uint64_t timeout, size_t count) {
        m_lastError = uc_emu_start(m_engine, begin, end, timeout, count);
        return m_lastError;
    }
    uc_err Emulator::StopEmulation() {
        m_lastError = uc_emu_stop(m_engine);
        return m_lastError;
    }

    uc_err Emulator::AddHook(int hookType, void* cb, void* userData, uc_hook* hk, uintptr_t begin, uintptr_t end) {
        uc_hook hook = 0;
        m_lastError = uc_hook_add(m_engine, &hook, hookType, cb, userData, begin, end);
        if (m_lastError == UC_ERR_OK) {
            m_unicornHooks.push_back(hook);
            if (hk)
                *hk = hook;
        }
        return m_lastError;
    }

    uc_err Emulator::DelHook(uc_hook hook) {
        m_lastError = uc_hook_del(m_engine, hook);
        return m_lastError;
    }

    uc_err Emulator::Query(int query, size_t* result) {
        m_lastError = uc_query(m_engine, (uc_query_type)query, result);
        return m_lastError;
    }
    uc_err Emulator::Ctl(int ctl_type, ...) {
        va_list args;
        va_start(args, ctl_type);
        m_lastError = uc_ctl(m_engine, (uc_control_type)ctl_type, args);
        va_end(args);
        return m_lastError;
    }

    uc_err Emulator::AllocContext(uc_context** context) {
        m_lastError = uc_context_alloc(m_engine, context);
        return m_lastError;
    }
    uc_err Emulator::FreeContext(uc_context* context) {
        m_lastError = uc_context_free(context);
        return m_lastError;
    }
    uc_err Emulator::SaveContext(uc_context* context) {
        m_lastError = uc_context_save(m_engine, context);
        return m_lastError;
    }
    uc_err Emulator::RestoreContext(uc_context* context) {
        m_lastError = uc_context_restore(m_engine, context);
        return m_lastError;
    }
    uc_err Emulator::ContextRegWrite(uc_context* ctx, int regid, const void* value) {
        m_lastError = uc_context_reg_write(ctx, regid, value);
        return m_lastError;
    }
    uc_err Emulator::ContextRegRead(uc_context* ctx, int regid, void* value) {
        m_lastError = uc_context_reg_read(ctx, regid, value);
        return m_lastError;
    }
    uc_err Emulator::ContextRegWrite2(uc_context* ctx, int regid, const void* value, size_t* size) {
        m_lastError = uc_context_reg_write2(ctx, regid, value, size);
        return m_lastError;
    }
    uc_err Emulator::ContextRegRead2(uc_context* ctx, int regid, void* value, size_t* size) {
        m_lastError = uc_context_reg_read2(ctx, regid, value, size);
        return m_lastError;
    }
    uc_err Emulator::ContextRegWriteBatch(uc_context* ctx, int* regs, void* const* vals, int count) {
        m_lastError = uc_context_reg_write_batch(ctx, regs, vals, count);
        return m_lastError;
    }
    uc_err Emulator::ContextRegReadBatch(uc_context* ctx, int* regs, void** vals, int count) {
        m_lastError = uc_context_reg_read_batch(ctx, regs, vals, count);
        return m_lastError;
    }
    uc_err Emulator::ContextRegWriteBatch2(uc_context* ctx, int* regs, const void* const* vals, size_t* sizes, int count) {
        m_lastError = uc_context_reg_write_batch2(ctx, regs, vals, sizes, count);
        return m_lastError;
    }
    uc_err Emulator::ContextRegReadBatch2(uc_context* ctx, int* regs, void** vals, size_t* sizes, int count) {
        m_lastError = uc_context_reg_read_batch2(ctx, regs, vals, sizes, count);
        return m_lastError;
    }
    uc_err Emulator::ContextSize(size_t* sz) const {
        *sz = uc_context_size(m_engine);
        return UC_ERR_OK;
    }

    uc_engine* Emulator::GetEngine() const { return m_engine; }
    uc_arch Emulator::GetArch() const { return m_arch; }
    uc_mode Emulator::GetMode() const { return m_mode; }
}
