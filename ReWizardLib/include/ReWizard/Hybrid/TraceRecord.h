#ifndef HYBRID_TRACE_RECORD_H
#define HYBRID_TRACE_RECORD_H

#include <Zydis/Zydis.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ReWizard {

    struct MemoryAccess {
        uintptr_t address = 0;
        uintptr_t value = 0;
        bool isWrite = false;
    };

    struct TraceRecord {
        uintptr_t pc = 0;
        std::map<ZydisRegister, uintptr_t> registers;
        std::vector<MemoryAccess> memoryAccesses;
    };

}

#endif
