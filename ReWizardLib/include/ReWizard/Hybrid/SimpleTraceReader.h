#ifndef HYBRID_SIMPLE_TRACE_READER_H
#define HYBRID_SIMPLE_TRACE_READER_H

#include <ReWizard/Hybrid/ITraceReader.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace ReWizard {

    class SimpleTraceReader : public ITraceReader {
    public:
        SimpleTraceReader() = default;
        ~SimpleTraceReader() = default;

        bool Load(const std::string& path) override;
        const std::vector<TraceRecord>& GetRecords() const override { return m_records; }
        std::vector<const TraceRecord*> GetRecordsForPC(uintptr_t pc) const override;

    private:
        std::vector<TraceRecord> m_records;
        std::unordered_map<uintptr_t, std::vector<size_t>> m_pcIndex;
    };

}

#endif
