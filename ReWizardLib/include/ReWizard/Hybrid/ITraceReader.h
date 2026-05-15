#ifndef HYBRID_I_TRACE_READER_H
#define HYBRID_I_TRACE_READER_H

#include <ReWizard/Hybrid/TraceRecord.h>
#include <string>
#include <vector>

namespace ReWizard {

    class ITraceReader {
    public:
        virtual ~ITraceReader() = default;

        virtual bool Load(const std::string& path) = 0;
        virtual const std::vector<TraceRecord>& GetRecords() const = 0;
        virtual std::vector<const TraceRecord*> GetRecordsForPC(uintptr_t pc) const = 0;
    };

}

#endif
