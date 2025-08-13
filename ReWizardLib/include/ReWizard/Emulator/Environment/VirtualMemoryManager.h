#ifndef EMULATOR_ENVIRONMENT_VMEM_MGR_H
#define EMULATOR_ENVIRONMENT_VMEM_MGR_H

#include <cstdint>
#include <memory>
#include <span>
#include <map>

#include <unicorn/unicorn.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


namespace ReWizard {

#ifdef _MSC_VER
	// TODO msvc allocator here and ideally, a map of custom flags to mmap and valloc flags
#else
#endif



	class VirtualRegion {
	public:
		enum class Perms : uint32_t {
			Read = UC_PROT_READ,
			Write = UC_PROT_WRITE,
			Execute = UC_PROT_EXEC,
			RW = Read|Write,
			RWX = UC_PROT_ALL,
			PERM_MAX,
		};

		enum class Type {
			SystemRegion,
			DynamicRegion,
			Module,
			TYPE_MAX,
		};

		~VirtualRegion() {
			if (m_isOwned && m_raw)
				VirtualFree(m_raw, 0, MEM_RELEASE);
		}

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

		uintptr_t			VirtualAddress() const { return m_virtualAddress; }
		uintptr_t			RealAddress() const { return m_realAddress; }
		size_t				Size() const { return m_size; }
		Perms				GetPerms() const { return m_perms; }
		Type				GetType() const { return m_type; }
		std::span<uint8_t>	RawPtr() const { return { m_raw, m_size }; }

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
			} else {
				m_raw = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
				if (!m_raw) {
					m_raw = static_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
					if (!m_raw) return;
				}
				m_realAddress = reinterpret_cast<uintptr_t>(m_raw);
			}
		}


	protected:
		bool		m_isOwned;
		uintptr_t	m_virtualAddress;
		uintptr_t	m_realAddress;
		size_t		m_size;
		uint8_t*	m_raw;
		Perms		m_perms{ Perms::PERM_MAX };
		Type		m_type{ Type::TYPE_MAX };
	};

	class Emulator;

	class VirtualMemoryManager {
	public:
		VirtualMemoryManager(Emulator& emulator) : m_emulator(emulator) {}

		VirtualRegion* InitStack(size_t size, uintptr_t address = 0);
		VirtualRegion* InitHeap(size_t size, uintptr_t address = 0);
		VirtualRegion* InitSegment(size_t size, uintptr_t address = 0);

		VirtualRegion* AllocateModule(uint8_t* buf, size_t size, uintptr_t address = 0, bool owned = false);
		VirtualRegion* Allocate(size_t size, uintptr_t address = 0, VirtualRegion::Perms perms = VirtualRegion::Perms::RW,  bool owned = false);
		VirtualRegion* Allocate(uint8_t* buf, size_t size, uintptr_t address = 0, VirtualRegion::Perms perms = VirtualRegion::Perms::RW, bool owned = false);

	private:
		std::unique_ptr<VirtualRegion>						m_stack, m_heap, m_segment;
		std::map<uintptr_t, std::unique_ptr<VirtualRegion>> m_regions;
		Emulator&											m_emulator;
	};

}

#endif
