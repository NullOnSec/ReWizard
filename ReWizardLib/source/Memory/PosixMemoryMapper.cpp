#include <ReWizard/Memory/PosixMemoryMapper.h>

#include <sys/mman.h>
#include <cstring>

namespace ReWizard {

    uint8_t* PosixMemoryMapper::Map(size_t size, uintptr_t preferredBase, bool executable) {
        int prot = PROT_READ | PROT_WRITE;
        if (executable) prot |= PROT_EXEC;

        int flags = MAP_PRIVATE | MAP_ANONYMOUS;

        // Try at the preferred base first
        void* hint = reinterpret_cast<void*>(preferredBase);
        void* ptr = mmap(hint, size, prot, flags | MAP_FIXED, -1, 0);
        if (ptr == MAP_FAILED) {
            // Fallback: let the kernel choose the address
            ptr = mmap(nullptr, size, prot, flags, -1, 0);
        }

        return (ptr == MAP_FAILED) ? nullptr : static_cast<uint8_t*>(ptr);
    }

    void PosixMemoryMapper::Unmap(uint8_t* ptr, size_t size) {
        if (ptr && size > 0) {
            munmap(ptr, size);
        }
    }

    void PosixMemoryMapper::Zero(uint8_t* ptr, size_t size) {
        if (ptr && size > 0) {
            memset(ptr, 0, size);
        }
    }

}
