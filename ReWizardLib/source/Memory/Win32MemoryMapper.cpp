#include <ReWizard/Memory/Win32MemoryMapper.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ReWizard {

    uint8_t* Win32MemoryMapper::Map(size_t size, uintptr_t preferredBase, bool executable) {
        DWORD prot = executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
        auto* ptr = reinterpret_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<LPVOID>(preferredBase), size, MEM_RESERVE | MEM_COMMIT, prot));
        if (!ptr) {
            ptr = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, prot));
        }
        return ptr;
    }

    void Win32MemoryMapper::Unmap(uint8_t* ptr, size_t /*size*/) {
        if (ptr) {
            VirtualFree(ptr, 0, MEM_RELEASE);
        }
    }

    void Win32MemoryMapper::Zero(uint8_t* ptr, size_t size) {
        if (ptr && size > 0) {
            ZeroMemory(ptr, size);
        }
    }

}
