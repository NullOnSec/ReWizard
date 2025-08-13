#include <ReWizard/Types/Win32InternalTypes.hpp>
#include <ReWizard/Emulator/Environment/Win32Impl/Win32Environment.h>
#include <ReWizard/Utils/Utils.h>

#include <intrin.h>

#include <LIEF/PE.hpp>

#include <vector>

namespace ReWizard {

	static void KUSER_AdjacentDataScan();
	PEB* CopyPEB(PEB** _PEB);
	TEB* CopyTEB(TEB** _TEB);

	Win32Environment::Win32Environment(AnalysisContext* ctx, LiefPE pe)
		: IEnvironment(ctx),
		m_pe(pe),
		m_memManager(*this) { }

	std::unique_ptr<Win32Environment> Win32Environment::Create(AnalysisContext* context) {
		if (context->GetLoader()->Binary()->format() != LIEF::Binary::PE)
			return nullptr;

		auto pe = context->GetLoader()->Binary()->as<LIEF::PE::Binary>();

		return std::unique_ptr<Win32Environment>(new Win32Environment(context, pe));
	}

	bool Win32Environment::InitializeEnvironment() {
		auto heap = m_memManager.InitHeap(DEFAULT_HEAP_SIZE);
		if (!heap)
			return false;

		auto stack = m_memManager.InitStack(DEFAULT_STACK_SIZE, DEFAULT_STACK_ADDR);
		if (!stack)
			return false;

		auto segment = m_memManager.InitSegment(DEFAULT_SEGMENT_ADDR, MIN_PAGE_SIZE);
		if (!heap)
			return false;

		auto rstack = stack->RawPtr();

		auto rsp_value = reinterpret_cast<uintptr_t>((rstack.data()+rstack.size()) - (sizeof(uintptr_t) * 128));
		if (!WriteRegister(UC_X86_REG_RSP, rsp_value)) {
			return false;
		}

		//m_memManager.Allocate(DEFAULT_HEAP_SIZE, 0, VirtualRegion::Perms::RW, true);

		return true;
	}
	
	bool Win32Environment::AnalyzeFunction(Function* function) {
		return true;
	}

	bool Win32Environment::AnalyzeAll() {
		return true;
	}


	PEB* Win32Environment::MapPEB() {
		auto pebRegion = m_memManager.Allocate(sizeof(PEB));
		PEB* peb = reinterpret_cast<PEB*>(pebRegion->RawPtr().data());

		PEB* origPeb = CopyPEB(&peb);
		if (!peb) 
			return nullptr;

		PEB_LDR_DATA* origLdr = origPeb->Ldr;
		if (!origLdr) 
			return nullptr;

		peb->BeingDebugged = 0;
		peb->ImageBaseAddress = reinterpret_cast<LPVOID>(m_context->GetLoader()->CurrentImageBase());

		auto ldrRegion = m_memManager.Allocate(sizeof(PEB_LDR_DATA));
		peb->Ldr = reinterpret_cast<PEB_LDR_DATA*>(ldrRegion->RawPtr().data());
		memcpy(peb->Ldr, origLdr, sizeof(PEB_LDR_DATA));

		const auto& modules = file->GetResolvedImports();
		auto module_pred = [&](const LDR_DATA_TABLE_ENTRY* entry) {
			return modules.count(reinterpret_cast<uintptr_t>(entry->DllBase));
			};

		auto loadOrderRegion = memory_->HeapAlloc(sizeof(LIST_ENTRY));
		auto memOrderRegion = memory_->HeapAlloc(sizeof(LIST_ENTRY));
		auto initOrderRegion = memory_->HeapAlloc(sizeof(LIST_ENTRY));

		peb->Ldr->InLoadOrderModuleList = *reinterpret_cast<LIST_ENTRY*>(loadOrderRegion->GetStartPtr());
		peb->Ldr->InMemoryOrderModuleList = *reinterpret_cast<LIST_ENTRY*>(memOrderRegion->GetStartPtr());
		peb->Ldr->InInitializationOrderModuleList = *reinterpret_cast<LIST_ENTRY*>(initOrderRegion->GetStartPtr());

		CloneModuleList(&origLdr->InLoadOrderModuleList, reinterpret_cast<LIST_ENTRY*>(loadOrderRegion->GetStartPtr()), module_pred);
		CloneModuleList(&origLdr->InMemoryOrderModuleList, reinterpret_cast<LIST_ENTRY*>(memOrderRegion->GetStartPtr()), module_pred);
		CloneModuleList(&origLdr->InInitializationOrderModuleList, reinterpret_cast<LIST_ENTRY*>(initOrderRegion->GetStartPtr()), module_pred);

		//if (emulator_.MemWrite(X64_PEB_ADDR, &peb, sizeof(uintptr_t)) != UC_ERR_OK) {
		//	spdlog::error("Error writing PEB pointer!");
		//	return nullptr;
		//}

		return peb;
	}

	LIST_ENTRY* Win32Environment::CloneModuleList(LIST_ENTRY* origHead, LIST_ENTRY* newHead, ListFilter filter) {
		if (!origHead || !newHead)
			return nullptr;

		std::vector<LDR_DATA_TABLE_ENTRY*> clonedEntries;

		clonedEntries.push_back(CreateSynthEntry());

		PLIST_ENTRY curr = origHead->Flink;
		while (curr != origHead) {
			auto* origEntry = CONTAINING_RECORD(curr, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			if (filter(origEntry)) {
				auto clonedEntry = CloneTableEntry(origEntry);
				clonedEntries.push_back(clonedEntry);
			}
			curr = curr->Flink;
		}

		newHead->Flink = newHead;
		newHead->Blink = newHead;
		for (size_t i = 0; i < clonedEntries.size(); ++i) {
			auto* entry = &clonedEntries[i]->InLoadOrderLinks;
			PLIST_ENTRY flink = (i + 1 < clonedEntries.size()) ? &clonedEntries[i + 1]->InLoadOrderLinks : newHead;
			PLIST_ENTRY blink = (i > 0) ? &clonedEntries[i - 1]->InLoadOrderLinks : newHead;
			entry->Flink = flink;
			entry->Blink = blink;
		}
		if (!clonedEntries.empty()) {
			newHead->Flink = &clonedEntries[0]->InLoadOrderLinks;
			newHead->Blink = &clonedEntries.back()->InLoadOrderLinks;
		}

		return newHead;
	}

	UNICODE_STRING Win32Environment::CloneUnicodeString(const UNICODE_STRING& src) {
		UNICODE_STRING dest = { 0 };
		if (src.Buffer && src.Length) {
			auto strRegion = m_memManager.Allocate(src.Length + sizeof(WCHAR));
			dest.Buffer = reinterpret_cast<WCHAR*>(strRegion->RawPtr().data());
			memcpy(dest.Buffer, src.Buffer, src.Length);
			dest.Buffer[src.Length / sizeof(WCHAR)] = 0;
			dest.Length = src.Length;
			dest.MaximumLength = src.Length + sizeof(WCHAR);
		}
		return dest;
	}

	LDR_DATA_TABLE_ENTRY* Win32Environment::CloneTableEntry(const LDR_DATA_TABLE_ENTRY* orig) {
		auto region = m_memManager.Allocate(sizeof(LDR_DATA_TABLE_ENTRY));
		auto* entry = reinterpret_cast<LDR_DATA_TABLE_ENTRY*>(region->RawPtr().data());
		memset(entry, 0, sizeof(LDR_DATA_TABLE_ENTRY));
		entry->DllBase = orig->DllBase;
		entry->SizeOfImage = orig->SizeOfImage;
		entry->EntryPoint = orig->EntryPoint;

		if (orig->BaseDllName.Buffer && orig->BaseDllName.Length)
			entry->BaseDllName = CloneUnicodeString(orig->BaseDllName);

		if (orig->FullDllName.Buffer && orig->FullDllName.Length)
			entry->FullDllName = CloneUnicodeString(orig->FullDllName);

		return entry;
	}

	LDR_DATA_TABLE_ENTRY* Win32Environment::CreateSynthEntry() {
		auto region = m_memManager.Allocate(sizeof(LDR_DATA_TABLE_ENTRY));
		auto* entry = reinterpret_cast<LDR_DATA_TABLE_ENTRY*>(region->RawPtr().data());
		memset(entry, 0, sizeof(LDR_DATA_TABLE_ENTRY));
		entry->DllBase = reinterpret_cast<void*>(m_pe->imagebase());

		const std::wstring baseName = m_context->GetLoader()->GetBaseNameW();
		if (!baseName.empty()) {
			UNICODE_STRING baseNameU;
			baseNameU.Buffer = (PWSTR)baseName.c_str();
			baseNameU.Length = static_cast<USHORT>(baseName.size() * sizeof(WCHAR));
			baseNameU.MaximumLength = static_cast<USHORT>((baseName.size() + 1) * sizeof(WCHAR));
			entry->BaseDllName = CloneUnicodeString(baseNameU);
		}

		const std::wstring fullName = m_context->GetLoader()->GetFullPathW();
		if (!fullName.empty()) {
			UNICODE_STRING fullNameU;
			fullNameU.Buffer = (PWSTR)fullName.c_str();
			fullNameU.Length = static_cast<USHORT>(fullName.size() * sizeof(WCHAR));
			fullNameU.MaximumLength = static_cast<USHORT>((fullName.size() + 1) * sizeof(WCHAR));
			entry->FullDllName = CloneUnicodeString(fullNameU);
		}
		return entry;
	}

	void KUSER_AdjacentDataScan() {
		constexpr uint64_t kStart = KUSER_SHARED_DATA_ADDR + KUSER_SHARED_DATA_SIZE;
		constexpr uint64_t kEnd = 0x80000000; // Arbitrary limit

		KUSER_ADJACENT_DATA_ADDR = 0;
		KUSER_ADJACENT_DATA_SIZE = 0;

		MEMORY_BASIC_INFORMATION mbi{};
		for (uint64_t addr = kStart; addr < kEnd; addr += mbi.RegionSize) {
			if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) != sizeof(mbi))
				break;

			if ((mbi.State == MEM_COMMIT || mbi.State == MEM_RESERVE) &&
				mbi.Type == MEM_PRIVATE &&
				mbi.Protect == PAGE_READONLY &&
				mbi.RegionSize == 0x1000) {

				KUSER_ADJACENT_DATA_ADDR = reinterpret_cast<uint64_t>(mbi.BaseAddress);
				KUSER_ADJACENT_DATA_SIZE = mbi.RegionSize;
				break;
			}
		}
	}

	PEB* CopyPEB(PEB** _PEB) {
		if (!_PEB) return nullptr;
		PEB* c_PEB = *_PEB, * o_PEB = nullptr;
		if (!c_PEB)
			c_PEB = new PEB;

		o_PEB = reinterpret_cast<PEB*>(__readgsqword(0x60));
		if (!c_PEB || !o_PEB) {
			if (c_PEB) delete c_PEB;
			return nullptr;
		}

		memcpy(c_PEB, o_PEB, sizeof(PEB));

		return o_PEB;
	}

	TEB* CopyTEB(TEB** _TEB) {
		if (!_TEB) return nullptr;
		TEB* c_TEB = *_TEB, * o_TEB = nullptr;
		if (!c_TEB)
			c_TEB = new TEB;

		o_TEB = reinterpret_cast<TEB*>(__readgsqword(0x30));
		if (!c_TEB || !o_TEB) {
			if (c_TEB) delete c_TEB;
			return nullptr;
		}

		memcpy(c_TEB, o_TEB, sizeof(TEB));

		return o_TEB;
	}

}
