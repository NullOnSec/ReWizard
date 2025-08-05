#include <Zydis/Zydis.h>
#include <cstdint>
#include <memory>
#include <map>
#include <mutex>

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

	namespace {
		class InternalDisassembler {
		public:
			~InternalDisassembler() = default;

			static InternalDisassembler* Get(ZydisMachineMode md, ZydisStackWidth sw);

		private:
			InternalDisassembler(ZydisMachineMode md, ZydisStackWidth sw);

			ZydisDecoder m_decoder;
			ZydisFormatter m_formatter;

			static std::map<std::pair<uintptr_t, uintptr_t>, std::unique_ptr<InternalDisassembler>> instances;
		};
	} // namespace internal

	class Disassembler {
	public:
		static std::unique_ptr<Disassembler> Get(ZydisMachineMode md, ZydisStackWidth sw);

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
	};

} // namespace ReWizard
