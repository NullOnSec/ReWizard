#include <ReWizard/Memory/MemoryMapper.h>

#ifdef _WIN32
#include <ReWizard/Memory/Win32MemoryMapper.h>
#else
#include <ReWizard/Memory/PosixMemoryMapper.h>
#endif

namespace ReWizard {

    std::unique_ptr<MemoryMapper> MemoryMapper::Create() {
#ifdef _WIN32
        return std::make_unique<Win32MemoryMapper>();
#else
        return std::make_unique<PosixMemoryMapper>();
#endif
    }

}
