#include <ReWizard/Hybrid/BochsInstrumentationBridge.h>
#include <ReWizard/Hybrid/IEmulator.h>
#include <ReWizard/Hybrid/TraceRecord.h>
#include <spdlog/spdlog.h>
#include <cstdint>
#include <vector>
#include <map>

// Bochs headers
#include <bochs.h>
#include <cpu/cpu.h>
#include <memory/memory-bochs.h>
#include <pc_system.h>

namespace ReWizard {

    static ITraceProducer* g_traceProducer = nullptr;
    static bool g_traceActive = false;

    void ReWizard_SetBochsTraceProducer(ITraceProducer* producer) {
        g_traceProducer = producer;
    }

    void ReWizard_SetBochsTraceActive(bool active) {
        g_traceActive = active;
    }

}

using namespace ReWizard;

// Bochs instrumentation callbacks
// These replace the stubs from stubs.lib

void bx_instr_init_env(void) {}
void bx_instr_exit_env(void) {}

void bx_instr_initialize(unsigned cpu) { (void)cpu; }
void bx_instr_exit(unsigned cpu) { (void)cpu; }
void bx_instr_reset(unsigned cpu, unsigned type) { (void)cpu; (void)type; }
void bx_instr_hlt(unsigned cpu) { (void)cpu; }
void bx_instr_mwait(unsigned cpu, bx_phy_address addr, unsigned len, Bit32u flags) {
    (void)cpu; (void)addr; (void)len; (void)flags;
}

void bx_instr_debug_promt() {}
void bx_instr_debug_cmd(const char *cmd) { (void)cmd; }

void bx_instr_cnear_branch_taken(unsigned cpu, bx_address branch_eip, bx_address new_eip) {
    (void)cpu; (void)branch_eip; (void)new_eip;
}
void bx_instr_cnear_branch_not_taken(unsigned cpu, bx_address branch_eip) {
    (void)cpu; (void)branch_eip;
}
void bx_instr_ucnear_branch(unsigned cpu, unsigned what, bx_address branch_eip, bx_address new_eip) {
    (void)cpu; (void)what; (void)branch_eip; (void)new_eip;
}
void bx_instr_far_branch(unsigned cpu, unsigned what, Bit16u prev_cs, bx_address prev_eip, Bit16u new_cs, bx_address new_eip) {
    (void)cpu; (void)what; (void)prev_cs; (void)prev_eip; (void)new_cs; (void)new_eip;
}

void bx_instr_opcode(unsigned cpu, bxInstruction_c *i, const Bit8u *opcode, unsigned len, bool is32, bool is64) {
    (void)cpu; (void)i; (void)opcode; (void)len; (void)is32; (void)is64;
}

void bx_instr_interrupt(unsigned cpu, unsigned vector) { (void)cpu; (void)vector; }
void bx_instr_exception(unsigned cpu, unsigned vector, unsigned error_code) { (void)cpu; (void)vector; (void)error_code; }
void bx_instr_hwinterrupt(unsigned cpu, unsigned vector, Bit16u cs, bx_address eip) { (void)cpu; (void)vector; (void)cs; (void)eip; }

void bx_instr_tlb_cntrl(unsigned cpu, unsigned what, bx_phy_address new_cr3) { (void)cpu; (void)what; (void)new_cr3; }
void bx_instr_clflush(unsigned cpu, bx_address laddr, bx_phy_address paddr) { (void)cpu; (void)laddr; (void)paddr; }
void bx_instr_cache_cntrl(unsigned cpu, unsigned what) { (void)cpu; (void)what; }
void bx_instr_prefetch_hint(unsigned cpu, unsigned what, unsigned seg, bx_address offset) { (void)cpu; (void)what; (void)seg; (void)offset; }

void bx_instr_before_execution(unsigned cpu, bxInstruction_c *i) {
    (void)i;

    if (!g_traceActive || !g_traceProducer)
        return;

    // Capture current CPU state from Bochs
    TraceRecord record;
    record.pc = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RIP].rrx);

    // Capture general-purpose registers
    record.registers[ZYDIS_REGISTER_RAX] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RAX].rrx);
    record.registers[ZYDIS_REGISTER_RCX] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RCX].rrx);
    record.registers[ZYDIS_REGISTER_RDX] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RDX].rrx);
    record.registers[ZYDIS_REGISTER_RBX] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RBX].rrx);
    record.registers[ZYDIS_REGISTER_RSP] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RSP].rrx);
    record.registers[ZYDIS_REGISTER_RBP] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RBP].rrx);
    record.registers[ZYDIS_REGISTER_RSI] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RSI].rrx);
    record.registers[ZYDIS_REGISTER_RDI] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_RDI].rrx);
    record.registers[ZYDIS_REGISTER_R8]  = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R8].rrx);
    record.registers[ZYDIS_REGISTER_R9]  = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R9].rrx);
    record.registers[ZYDIS_REGISTER_R10] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R10].rrx);
    record.registers[ZYDIS_REGISTER_R11] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R11].rrx);
    record.registers[ZYDIS_REGISTER_R12] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R12].rrx);
    record.registers[ZYDIS_REGISTER_R13] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R13].rrx);
    record.registers[ZYDIS_REGISTER_R14] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R14].rrx);
    record.registers[ZYDIS_REGISTER_R15] = static_cast<uintptr_t>(BX_CPU(cpu)->gen_reg[BX_64BIT_REG_R15].rrx);

    g_traceProducer->OnTraceRecord(record);
}

void bx_instr_after_execution(unsigned cpu, bxInstruction_c *i) {
    (void)cpu; (void)i;

    if (!g_traceActive || !g_traceProducer)
        return;

    // Post-execution state capture if needed
}

void bx_instr_repeat_iteration(unsigned cpu, bxInstruction_c *i) { (void)cpu; (void)i; }

void bx_instr_inp(Bit16u addr, unsigned len) { (void)addr; (void)len; }
void bx_instr_inp2(Bit16u addr, unsigned len, unsigned val) { (void)addr; (void)len; (void)val; }
void bx_instr_outp(Bit16u addr, unsigned len, unsigned val) { (void)addr; (void)len; (void)val; }

void bx_instr_lin_access(unsigned cpu, bx_address lin, bx_address phy, unsigned len, unsigned memtype, unsigned rw) {
    (void)cpu; (void)lin; (void)phy; (void)len; (void)memtype; (void)rw;

    if (!g_traceActive || !g_traceProducer)
        return;

    // Memory access tracing
    // TODO: Add OnMemoryAccess callback to ITraceProducer if needed
}

void bx_instr_phy_access(unsigned cpu, bx_address phy, unsigned len, unsigned memtype, unsigned rw) {
    (void)cpu; (void)phy; (void)len; (void)memtype; (void)rw;
}

void bx_instr_wrmsr(unsigned cpu, unsigned addr, Bit64u value) { (void)cpu; (void)addr; (void)value; }
void bx_instr_vmexit(unsigned cpu, Bit32u reason, Bit64u qualification) { (void)cpu; (void)reason; (void)qualification; }
