#include <ReWizard/Emulator/Environment/VirtualMemoryManager.h>
#include <ReWizard/Emulator/Emulator.h>

namespace ReWizard {

	VirtualRegion* VirtualMemoryManager::InitStack(size_t size, uintptr_t address) {
		auto region = VirtualRegion::Create(address, size, VirtualRegion::Perms::RW, VirtualRegion::Type::SystemRegion);

		if (!region)
			return nullptr;

		if (m_emulator.MapMemoryPtr(address, region->RawPtr().data(), size, (uint32_t)region->GetPerms()) != UC_ERR_OK) {
			m_stack.reset(nullptr);
			return nullptr;
		}

		m_stack = std::move(region);

		return m_stack.get();
	}

	VirtualRegion* VirtualMemoryManager::InitHeap(size_t size, uintptr_t address) {
		auto region = VirtualRegion::Create(address, size, VirtualRegion::Perms::RW, VirtualRegion::Type::SystemRegion);

		if (!region)
			return nullptr;

		if (m_emulator.MapMemoryPtr(address, region->RawPtr().data(), size, (uint32_t)region->GetPerms()) != UC_ERR_OK) {
			m_heap.reset(nullptr);
			return nullptr;
		}

		m_heap = std::move(region);

		return m_heap.get();
	}

	VirtualRegion* VirtualMemoryManager::InitSegment(size_t size, uintptr_t address) {
		auto region = VirtualRegion::Create(address, size, VirtualRegion::Perms::RW, VirtualRegion::Type::SystemRegion);

		if (!region)
			return nullptr;

		if (m_emulator.MapMemoryPtr(address, region->RawPtr().data(), size, (uint32_t)region->GetPerms()) != UC_ERR_OK) {
			return nullptr;
		}

		m_segment = std::move(region);

		return m_segment.get();
	}

	VirtualRegion* VirtualMemoryManager::Allocate(size_t size, uintptr_t address, VirtualRegion::Perms perms, bool owned) {
		auto type = VirtualRegion::Type::DynamicRegion;

		auto region = owned
			? VirtualRegion::Create(address, size, perms, type)
			: VirtualRegion::Create(reinterpret_cast<uint8_t*>(address), size, perms, type);

		if (!region)
			return nullptr;

		auto ptr = region.get();

		if (m_emulator.MapMemoryPtr(address, ptr->RawPtr().data(), size, (uint32_t)ptr->GetPerms()) != UC_ERR_OK)
			return nullptr;

		if (owned)
			memset(ptr->RawPtr().data(), 0, size);

		m_regions.insert({ address, std::move(region) });
		return ptr;
	}

	VirtualRegion* VirtualMemoryManager::Allocate(uint8_t* buf, size_t size, uintptr_t address, VirtualRegion::Perms perms, bool owned) {
		auto type = VirtualRegion::Type::DynamicRegion;

		auto region = owned
			? VirtualRegion::Create(address, size, perms, type)
			: VirtualRegion::Create(buf, size, perms, type);

		if (!region)
			return nullptr;

		auto ptr = region.get();

		if (m_emulator.MapMemoryPtr(address, ptr->RawPtr().data(), size, (uint32_t)ptr->GetPerms()) != UC_ERR_OK)
			return nullptr;

		if (owned)
			memcpy(ptr->RawPtr().data(), buf, size);

		m_regions.insert({ address, std::move(region) });
		return ptr;
	}
}
