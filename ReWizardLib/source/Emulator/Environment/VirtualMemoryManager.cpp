#include <ReWizard/Emulator/Environment/VirtualMemoryManager.h>

namespace ReWizard {

	bool VirtualMemoryManager::InitStack(size_t size, uintptr_t address) {
		auto region = VirtualRegion::Create(size, VirtualRegion::Perms::RW, address);

		if (!region)
			return false;

		m_stack = std::move(region);

		return true;
	}

	bool VirtualMemoryManager::InitHeap(size_t size, uintptr_t address) {
		return true;
	}

	bool VirtualMemoryManager::InitSegment(size_t size, uintptr_t address) {
		return true;
	}

}

