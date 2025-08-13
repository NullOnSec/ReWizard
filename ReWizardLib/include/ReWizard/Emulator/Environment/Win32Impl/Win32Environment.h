#ifndef WIN32_EMULATION_ENVIRONMENT_H
#define WIN32_EMULATION_ENVIRONMENT_H

#include <ReWizard/Emulator/Environment/IEnvironment.hpp>
#include <ReWizard/Emulator/Environment/VirtualMemoryManager.h>
#include <memory>

namespace ReWizard {
	// Opaque Win32 structure forward declarations
	//struct _LIST_ENTRY;
	//struct _LDR_DATA_TABLE_ENTRY;
	//struct _UNICODE_STRING;
	//
	//using LIST_ENTRY = _LIST_ENTRY;
	//using LDR_DATA_TABLE_ENTRY = _LDR_DATA_TABLE_ENTRY;
	//using UNICODE_STRING = _UNICODE_STRING;

	using ListFilter = bool(*)(const LDR_DATA_TABLE_ENTRY*);

	using LiefPE = LIEF::Object::output_t<LIEF::PE::Binary>;

	const uint64_t DEFAULT_HEAP_SIZE = 32ull * 1024 * 1024; // 32MB
	const uint64_t DEFAULT_STACK_SIZE = 8ull * 1024 * 1024; // 8MB
	const uint64_t DEFAULT_STACK_ADDR = 0x7000000000ull;
	const uint64_t DEFAULT_SEGMENT_ADDR = 0x00;
	const uint64_t MIN_PAGE_SIZE = 0x1000;

	const uint64_t KUSER_SHARED_DATA_ADDR = 0x7FFE0000;
	const uint64_t KUSER_SHARED_DATA_SIZE = MIN_PAGE_SIZE;
	inline uint64_t KUSER_ADJACENT_DATA_ADDR = 0x7FFEF000;
	inline uint64_t KUSER_ADJACENT_DATA_SIZE = MIN_PAGE_SIZE;

	const uint64_t X64_PEB_ADDR = 0x60;
	const uint64_t X64_TEB_ADDR = 0x30;

	class Win32Environment : public IEnvironment {
	public:
		static std::unique_ptr<Win32Environment> Create(AnalysisContext* context);

		bool InitializeEnvironment() override;
		bool AnalyzeFunction(Function* function) override;
		bool AnalyzeAll() override;

	private:
		Win32Environment(AnalysisContext* ctx, LiefPE pe);

		// Cloning helpers
		LDR_DATA_TABLE_ENTRY* CreateSynthEntry();
		LDR_DATA_TABLE_ENTRY* CloneTableEntry(const LDR_DATA_TABLE_ENTRY* orig);
		UNICODE_STRING CloneUnicodeString(const UNICODE_STRING& src);
		LIST_ENTRY* CloneModuleList(LIST_ENTRY* origHead, LIST_ENTRY* newHead, ListFilter filter);
		PEB* MapPEB();

	private:
		VirtualMemoryManager m_memManager;
		LiefPE m_pe;
	};
}

#endif
