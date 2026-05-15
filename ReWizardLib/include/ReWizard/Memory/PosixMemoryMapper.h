#ifndef POSIX_MEMORY_MAPPER_H
#define POSIX_MEMORY_MAPPER_H

#include <ReWizard/Memory/MemoryMapper.h>

namespace ReWizard {

    class PosixMemoryMapper : public MemoryMapper {
    public:
        uint8_t* Map(size_t size, uintptr_t preferredBase, bool executable) override;
        void Unmap(uint8_t* ptr, size_t size) override;
        void Zero(uint8_t* ptr, size_t size) override;
    };

}

#endif
