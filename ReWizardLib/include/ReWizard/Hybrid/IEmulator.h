#ifndef I_EMULATOR_H
#define I_EMULATOR_H

#include <ReWizard/Hybrid/TraceRecord.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ReWizard {

    class ITraceProducer {
    public:
        virtual ~ITraceProducer() = default;

        virtual void OnTraceRecord(const TraceRecord& record) = 0;
        virtual void OnExecutionComplete() = 0;
    };

    class IEmulator {
    public:
        virtual ~IEmulator() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual bool LoadBinary(const uint8_t* data, size_t len, uintptr_t baseAddress) = 0;

        virtual bool SetRegister(ZydisRegister reg, uintptr_t value) = 0;
        virtual bool GetRegister(ZydisRegister reg, uintptr_t& value) = 0;

        virtual bool Snapshot(const std::string& name) = 0;
        virtual bool Restore(const std::string& name) = 0;

        virtual bool Execute(uintptr_t start, size_t maxInstructions = 0) = 0;

        virtual void SetTraceProducer(ITraceProducer* producer) = 0;

        virtual std::string Name() const = 0;
    };

}

#endif
