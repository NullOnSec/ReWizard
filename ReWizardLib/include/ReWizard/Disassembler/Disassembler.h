#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <Zydis/Zydis.h>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <memory>

namespace ReWizard {

    enum class InstructionType {
        DecodedInstruction,
        ExtendedInstruction,
    };

    class DecodedInstruction {
    public:
        static std::unique_ptr<DecodedInstruction> Create();
        ~DecodedInstruction() = default;

        ZydisDecodedInstruction& Instruction() { return m_instruction; }
        ZydisDecodedOperand* Operands() { return m_operands; }
        const ZydisDecodedOperand* Operands() const { return m_operands; }
        const uintptr_t Address() const { return m_addr; }
        uintptr_t& Address() { return m_addr; }
        InstructionType Type() { return m_type; }

    protected:
        DecodedInstruction() = default;

    protected:
        ZydisDecodedInstruction m_instruction = {};
        ZydisDecodedOperand m_operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        uintptr_t m_addr = {};
        InstructionType m_type = {InstructionType::DecodedInstruction};
    };

    class ExtendedInstruction : public DecodedInstruction {
    public:
        static std::unique_ptr<ExtendedInstruction> Create();
        ~ExtendedInstruction() = default;

        bool IsIndirect() const { return m_isIndirect; }
        uintptr_t& IndirectValue() { return m_indirectValue; }
        const uintptr_t IndirectValue() const { return m_indirectValue; }

    private:
        ExtendedInstruction() = default;

    private:
        bool m_isIndirect = false;
        uintptr_t m_indirectValue = { 0 };
    };

    class InternalDisassembler {
    public:
        ~InternalDisassembler() = default;

        static InternalDisassembler* Get(ZydisMachineMode md, ZydisStackWidth sw);

        template<typename T,
            std::enable_if_t<
            std::is_same_v<T, DecodedInstruction> || std::is_same_v<T, ExtendedInstruction>,
            int> = 0>
        std::unique_ptr<T> DisassembleSingle(uint8_t * data, size_t dataSz);
        std::string InstructionToString(DecodedInstruction* insn, uintptr_t baseAddr, bool withAddr = true);

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

#endif
