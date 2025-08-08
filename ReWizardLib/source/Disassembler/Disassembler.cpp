#include <ReWizard/Disassembler/Disassembler.h>
#include <iomanip>
#include <sstream>

namespace ReWizard {

    std::unique_ptr<DecodedInstruction> DecodedInstruction::Create() {
        auto insn = std::unique_ptr<DecodedInstruction>(new DecodedInstruction);;
        insn->m_type = InstructionType::DecodedInstruction;
        return insn;
    }

    std::unique_ptr<ExtendedInstruction> ExtendedInstruction::Create() {
        auto insn = std::unique_ptr<ExtendedInstruction>(new ExtendedInstruction);;
        insn->m_type = InstructionType::ExtendedInstruction;
        return insn;
    }


    std::map<std::pair<uintptr_t, uintptr_t>, std::unique_ptr<InternalDisassembler>> InternalDisassembler::instances;

    InternalDisassembler* InternalDisassembler::Get(ZydisMachineMode md, ZydisStackWidth sw) {
        auto key = std::make_pair(static_cast<uintptr_t>(md), static_cast<uintptr_t>(sw));
        auto it = instances.find(key);
        if (it != instances.end())
            return it->second.get();

        auto instance = std::unique_ptr<InternalDisassembler>(new InternalDisassembler(md, sw));
        InternalDisassembler* ptr = instance.get();
        instances.insert({ key, std::move(instance) });
        return ptr;
    }

    InternalDisassembler::InternalDisassembler(ZydisMachineMode md, ZydisStackWidth sw) {
        ZydisDecoderInit(&m_decoder, md, sw);
        ZydisFormatterInit(&m_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
    }

    template<
        typename T,
        std::enable_if_t<
            std::is_same_v<T, DecodedInstruction> || std::is_same_v<T, ExtendedInstruction>,
            int
        >
    >
    std::unique_ptr<T> InternalDisassembler::DisassembleSingle(uint8_t* data, size_t dataSz) {
        auto instruction = T::Create();
        if (ZydisDecoderDecodeFull(&m_decoder, data, dataSz, &instruction->Instruction(), instruction->Operands()) != ZYAN_STATUS_SUCCESS) {
            return nullptr;
        }
        instruction->Address() = reinterpret_cast<uintptr_t>(data);
        return instruction;
    }

    template std::unique_ptr<DecodedInstruction> InternalDisassembler::DisassembleSingle<DecodedInstruction>(uint8_t*, size_t);
    template std::unique_ptr<ExtendedInstruction> InternalDisassembler::DisassembleSingle<ExtendedInstruction>(uint8_t*, size_t);


    std::string InternalDisassembler::InstructionToString(DecodedInstruction* insn, uintptr_t baseAddr, bool withAddr) {
        if (!insn) {
            return "";
        }

        char buffer[256];
        ZydisFormatterFormatInstruction(
            &m_formatter,
            &insn->Instruction(),
            insn->Operands(),
            insn->Instruction().operand_count_visible,
            buffer,
            sizeof(buffer),
            baseAddr,
            ZYAN_NULL
        );

        if (withAddr) {
            std::stringstream ss;
            ss << "0x"
                << std::hex << std::setw(8) << std::setfill('0') << baseAddr
                << ": " << buffer;
            return ss.str();
        }
        return std::string(buffer);
    }

    std::mutex Disassembler::m_mutex;
    std::map<std::pair<uintptr_t, uintptr_t>, Disassembler*> Disassembler::s_instances; // <- Static map

    Disassembler& Disassembler::Get(ZydisMachineMode md, ZydisStackWidth sw) { // return reference
        auto key = std::make_pair(static_cast<uintptr_t>(md), static_cast<uintptr_t>(sw));
        auto it = s_instances.find(key);
        if (it != s_instances.end())
            return *it->second;

        InternalDisassembler* d = InternalDisassembler::Get(md, sw);
        auto* dis = new Disassembler(d);
        s_instances[key] = dis;
        return *dis;
    }

    Disassembler::Disassembler(InternalDisassembler* ptr)
        : m_dis(ptr) {
    }

    Disassembler::Proxy Disassembler::operator->() {
        return Proxy(m_dis, m_mutex);
    }

} // namespace ReWizard
