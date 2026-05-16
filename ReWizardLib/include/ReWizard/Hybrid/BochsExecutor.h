#ifndef BOCHS_EXECUTOR_H
#define BOCHS_EXECUTOR_H

#include <ReWizard/Hybrid/IEmulator.h>
#include <string>
#include <memory>

namespace ReWizard {

    class BochsExecutor : public IEmulator {
    public:
        BochsExecutor();
        ~BochsExecutor() override;

        bool Initialize() override;
        void Shutdown() override;

        bool LoadBinary(const uint8_t* data, size_t len, uintptr_t baseAddress) override;

        bool SetRegister(ZydisRegister reg, uintptr_t value) override;
        bool GetRegister(ZydisRegister reg, uintptr_t& value) override;

        bool Snapshot(const std::string& name) override;
        bool Restore(const std::string& name) override;

        bool Execute(uintptr_t start, size_t maxInstructions = 0) override;

        void SetTraceProducer(ITraceProducer* producer) override;

        std::string Name() const override { return "Bochs"; }

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}

#endif
