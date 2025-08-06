#pragma once

#include <Zydis/Zydis.h>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <memory>

namespace ReWizard {

    class BaseInstruction {
    public:
        static std::unique_ptr<BaseInstruction> Create();
        ~BaseInstruction() = default;

        ZydisDecodedInstruction instruction = {};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        uintptr_t addr = {};

    private:
        BaseInstruction() = default;
    };

    class InternalDisassembler {
    public:
        ~InternalDisassembler() = default;

        static InternalDisassembler* Get(ZydisMachineMode md, ZydisStackWidth sw);

        std::unique_ptr<BaseInstruction> DisassembleSingle(uint8_t* data, size_t dataSz);
        std::string InstructionToString(BaseInstruction* insn, uintptr_t baseAddr, bool withAddr = true);
        std::string InstructionToString(std::unique_ptr<BaseInstruction>& insn, uintptr_t baseAddr, bool withAddr = true);

    private:
        InternalDisassembler(ZydisMachineMode md, ZydisStackWidth sw);

        ZydisDecoder m_decoder;
        ZydisFormatter m_formatter;

        static std::map<std::pair<uintptr_t, uintptr_t>, std::unique_ptr<InternalDisassembler>> instances;
    };

    class Disassembler {
    public:
        static Disassembler& Get(ZydisMachineMode md, ZydisStackWidth sw); // now returns reference

        class Proxy {
        public:
            Proxy(InternalDisassembler* ptr, std::mutex& mtx)
                : m_ptr(ptr), m_lock(mtx) {
            }

            InternalDisassembler* operator->() { return m_ptr; }
        private:
            InternalDisassembler* m_ptr;
            std::unique_lock<std::mutex> m_lock;
        };

        Proxy operator->();

        Disassembler(const Disassembler&) = delete;
        Disassembler& operator=(const Disassembler&) = delete;

    private:
        explicit Disassembler(InternalDisassembler* ptr);

        InternalDisassembler* m_dis;
        static std::mutex m_mutex;
        static std::map<std::pair<uintptr_t, uintptr_t>, Disassembler*> s_instances;
    };

} // namespace ReWizard
