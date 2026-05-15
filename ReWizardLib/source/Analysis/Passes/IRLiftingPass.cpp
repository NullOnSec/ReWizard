#include <ReWizard/Analysis/Passes/IRLiftingPass.hpp>
#include <ReWizard/Analysis/AnalysisContext.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/IR/Lifter.h>
#include <ReWizard/FileLoader/FileLoader.h>

#include <spdlog/spdlog.h>

namespace ReWizard {

    bool IRLiftingPass::PreRun(AnalysisContext* context) {
        (void)context;
        return true;
    }

    bool IRLiftingPass::PostRun(AnalysisContext* context) {
        (void)context;
        return true;
    }

    bool IRLiftingPass::Run(AnalysisContext* context) {
        if (!context)
            return false;

        auto loader = context->GetLoader();
        if (!loader)
            return false;

        auto binary = loader->Binary();
        if (!binary)
            return false;

        auto module = context->GetModule();
        if (!module)
            return false;

        spdlog::info("IRLiftingPass: lifting functions to LLVM IR");

        bool is64Bit = (loader->Arch().first == ZYDIS_MACHINE_MODE_LONG_64);
        lifter_ = std::make_unique<Lifter>(is64Bit);

        size_t liftedBlocks = 0;
        size_t totalBlocks = 0;

        for (auto& function : module->GetFunctions()) {
            if (!function)
                continue;

            for (auto& bb : function->GetBasicBlocks()) {
                if (!bb)
                    continue;

                totalBlocks++;

                auto start = bb->GetStart();
                auto end = bb->GetEnd();
                if (end <= start)
                    continue;

                size_t len = end - start;
                auto raw = loader->Raw();
                auto imageBase = loader->CurrentImageBase();

                if (start < imageBase || start + len > imageBase + raw.size())
                    continue;

                size_t offset = start - imageBase;
                const uint8_t* bytes = raw.data() + offset;

                Lifter::Options opts;
                opts.baseAddress = start;
                opts.is64Bit = (loader->Arch().first == ZYDIS_MACHINE_MODE_LONG_64);

                auto result = lifter_->LiftBasicBlock(bytes, len, opts);

                if (result.success && result.function && result.entryBlock) {
                    auto irBlock = IRBlock::Wrap(result.entryBlock);
                    irBlock->SetNativeAddress(start);
                    irBlock->SetName(bb->GetName());
                    bb->SetIRBlock(std::move(irBlock));
                    liftedBlocks++;
                }
            }

            if (liftedBlocks > 0) {
                spdlog::debug("IRLiftingPass: function {} - lifted {}/{} blocks",
                              function->GetName(), liftedBlocks, totalBlocks);
            }
        }

        spdlog::info("IRLiftingPass: lifted {}/{} basic blocks to LLVM IR", liftedBlocks, totalBlocks);
        return true;
    }

}
