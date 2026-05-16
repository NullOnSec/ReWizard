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

        // Configure the path to a raw disk image for Windows boot.
        // Must be called before Initialize().
        void SetDiskImage(const std::string& path);

        // Boot from the configured disk image.
        // Runs the CPU from the BIOS reset vector; returns when icount is reached
        // or a guard fires. Call repeatedly to progress through boot.
        bool BootFromDisk(size_t maxInstructions);

        std::string Name() const override { return "Bochs"; }

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}

#endif
