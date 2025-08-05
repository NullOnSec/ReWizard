#include <ReWizard/Disassembler/Disassembler.h>

namespace ReWizard {

	std::unique_ptr<BaseInstruction> BaseInstruction::Create() {
		return std::unique_ptr<BaseInstruction>(new BaseInstruction);
	}

	namespace {
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
	} // namespace internal

	std::mutex Disassembler::m_mutex;

	std::unique_ptr<Disassembler> Disassembler::Get(ZydisMachineMode md, ZydisStackWidth sw) {
		InternalDisassembler* d = InternalDisassembler::Get(md, sw);
		return std::unique_ptr<Disassembler>(new Disassembler(d));
	}

	Disassembler::Disassembler(InternalDisassembler* ptr)
		: m_dis(ptr) {
	}

	Disassembler::Proxy Disassembler::operator->() {
		return Proxy(m_dis, m_mutex);
	}

} // namespace ReWizard
