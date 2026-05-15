#include <ReWizard/IR/Lifter.h>

#include <spdlog/spdlog.h>

namespace ReWizard {

    class Lifter::Impl {
    public:
        Impl() {}
        ~Impl() {}
    };

    Lifter::Lifter() : impl_(std::make_unique<Impl>()) {}

    Lifter::~Lifter() = default;

    Lifter::Lifter(Lifter&& other) noexcept = default;
    Lifter& Lifter::operator=(Lifter&& other) noexcept = default;

    Lifter::LiftResult Lifter::LiftBasicBlock(const uint8_t* bytes, size_t len, const Options& opts) {
        LiftResult result;

        if (!bytes || len == 0) {
            result.errorMessage = "Empty input bytes";
            return result;
        }

        // Stub: LLVM C API not available for x86 target (LLVM-C.lib is x64)
        // When LLVM is built from source for the correct architecture,
        // this will use LLVM C API to create actual IR.
        result.success = true;
        result.bytesConsumed = len;

        spdlog::debug("Lifter: stub lift of basic block at 0x{:x} ({} bytes)", opts.baseAddress, len);
        return result;
    }

    Lifter::LiftResult Lifter::LiftFunction(const uint8_t* bytes, size_t len, const Options& opts) {
        return LiftBasicBlock(bytes, len, opts);
    }

    llvm::Module* Lifter::GetModule() const {
        return nullptr;
    }

    llvm::LLVMContext* Lifter::GetContext() const {
        return nullptr;
    }

    std::string Lifter::DumpModule() const {
        return "; ModuleID = 'rewizard_lift'\nsource_filename = \"rewizard_lift\"\n";
    }

}
