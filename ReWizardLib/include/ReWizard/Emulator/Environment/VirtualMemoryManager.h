#ifndef EMULATOR_ENVIRONMENT_VMEM_MGR_H
#define EMULATOR_ENVIRONMENT_VMEM_MGR_H

#include <cstdint>
#include <memory>
#include <span>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>



namespace ReWizard {

#ifdef _MSC_VER
	// TODO msvc allocator here and ideally, a map of custom flags to mmap and valloc flags
#else
#endif

	const uint64_t DEFAULT_HEAP_SIZE = 32ull * 1024 * 1024; // 32MB Default heap
	const uint64_t DEFAULT_STACK_SIZE = 8ull * 1024 * 1024; // 8MB Default stack
	const uint64_t DEFAULT_STACK_ADDR = 0x7000000000ull;
	const uint64_t DEFAULT_SEGMENT_ADDR = 0x00;
	const uint64_t MIN_PAGE_SIZE = 0x1000;

	const uint64_t KUSER_SHARED_DATA_ADDR = 0x7FFE0000;
	const uint64_t KUSER_SHARED_DATA_SIZE = MIN_PAGE_SIZE;
	inline uint64_t KUSER_ADJACENT_DATA_ADDR = 0x7FFEF000;
	inline uint64_t KUSER_ADJACENT_DATA_SIZE = MIN_PAGE_SIZE;

	const uint64_t X64_PEB_ADDR = 0x60;
	const uint64_t X64_TEB_ADDR = 0x30;

	class VirtualRegion {
	public:
		enum class Perms : uint8_t {
			Read = 1 << 0,
			Write = 1 << 1,
			Execute = 1 << 2,
			RW = Read | Write,
			RWX = Read | Write | Execute
		};

		enum class Type {
			SystemRegion,
			Module,
		};

		static std::unique_ptr<VirtualRegion> Create(uint8_t* ptr, size_t size, Perms perms, Type type) {
			auto p = std::unique_ptr<VirtualRegion>(new VirtualRegion(size, perms, type, reinterpret_cast<uintptr_t>(ptr), false));
			auto s = p->RawPtr();
			if (s.size() == 0)
				return nullptr;
			return p;
		}

		static std::unique_ptr<VirtualRegion> Create(uintptr_t addr, size_t size, Perms perms, Type type) {
			auto p = std::unique_ptr<VirtualRegion>(new VirtualRegion(size, perms, type, addr, true));
			auto s = p->RawPtr();
			if (s.size() == 0)
				return nullptr;
			return p;
		}

		uintptr_t VirtualAddress() const { return m_virtualAddress; }
		uintptr_t RealAddress() const { return m_realAddress; }
		std::span<uint8_t> RawPtr() const { return { m_raw, m_size }; }
		size_t Size() const { return m_size; }

	private:
		VirtualRegion(size_t size, Perms perms, Type type, uintptr_t address, bool owned)
			: m_isOwned(owned),
			m_virtualAddress(address),
			m_realAddress(0),
			m_raw(nullptr),
			m_size(size),
			m_perms(perms),
			m_type(type)
		{
			if (!m_isOwned) {
				m_raw = reinterpret_cast<uint8_t*>(address);
				m_realAddress = m_virtualAddress;
			}
			else {
				m_raw = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
				if (!m_raw) {
					m_raw = static_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
					if (!m_raw) return;
				}
				m_realAddress = reinterpret_cast<uintptr_t>(m_raw);
			}
		}

		~VirtualRegion() {
			if (m_isOwned && m_raw)
				VirtualFree(m_raw, 0, MEM_RELEASE);
		}

		bool		m_isOwned;
		uintptr_t	m_virtualAddress;
		uintptr_t	m_realAddress;
		size_t		m_size;
		uint8_t*	m_raw;
		Perms		m_perms{};
		Type		m_type{};
	};


	class VirtualMemoryManager {
	public:
		VirtualMemoryManager() {}

		bool InitStack(size_t size, uintptr_t address = 0);
		bool InitHeap(size_t size, uintptr_t address = 0);
		bool InitSegment(size_t size, uintptr_t address = 0);

	private:
		std::unique_ptr<VirtualRegion> m_stack, m_heap, m_segment;
	};

}

#endif
