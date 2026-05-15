#ifndef MEMORY_MAPPER_H
#define MEMORY_MAPPER_H

#include <cstdint>
#include <cstddef>
#include <memory>

namespace ReWizard {

    class MemoryMapper {
    public:
        virtual ~MemoryMapper() = default;

        virtual uint8_t* Map(size_t size, uintptr_t preferredBase, bool executable) = 0;
        virtual void Unmap(uint8_t* ptr, size_t size) = 0;
        virtual void Zero(uint8_t* ptr, size_t size) = 0;

        static std::unique_ptr<MemoryMapper> Create();
    };

}

#endif
