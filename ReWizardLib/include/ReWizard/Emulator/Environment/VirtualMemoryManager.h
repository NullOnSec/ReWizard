#ifndef EMULATOR_ENVIRONMENT_VMEM_MGR_H
#define EMULATOR_ENVIRONMENT_VMEM_MGR_H

#include <cstdint>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ReWizard {

#ifdef _MSC_VER
	// TODO msvc allocator here and ideally, a map of custom flags to mmap and valloc flags
#else
#endif

	class VirtualRegion {

	};

	class VirtualMemoryManager {
	public:
		VirtualMemoryManager() {}

	private:
		std::unique_ptr<VirtualRegion> m_stack, m_heap, m_segment;
	};

}

#endif
