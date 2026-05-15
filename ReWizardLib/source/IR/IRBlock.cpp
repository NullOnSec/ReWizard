#include <ReWizard/IR/IRBlock.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

namespace ReWizard {

    class IRBlock::Impl {
    public:
        LLVMBasicBlockRef block = nullptr;
        bool ownsReference = false;
    };

    IRBlock::IRBlock() : impl_(std::make_unique<Impl>()) {}

    IRBlock::~IRBlock() {
        if (impl_ && impl_->block && impl_->ownsReference) {
            // LLVM doesn't provide a direct delete for basic blocks via C API
            // In real implementation with C++ API: delete block;
        }
    }

    IRBlock::IRBlock(IRBlock&& other) noexcept
        : impl_(std::move(other.impl_)), nativeAddr_(other.nativeAddr_), name_(std::move(other.name_)) {
        other.nativeAddr_ = 0;
    }

    IRBlock& IRBlock::operator=(IRBlock&& other) noexcept {
        if (this != &other) {
            impl_ = std::move(other.impl_);
            nativeAddr_ = other.nativeAddr_;
            name_ = std::move(other.name_);
            other.nativeAddr_ = 0;
        }
        return *this;
    }

    std::unique_ptr<IRBlock> IRBlock::Wrap(llvm::BasicBlock* bb) {
        // Stub: will be implemented when LLVM C++ API is available
        auto result = std::make_unique<IRBlock>();
        result->SetName("bb_stub");
        return result;
    }

    llvm::BasicBlock* IRBlock::GetLLVMBlock() const {
        // Stub: will be implemented when LLVM C++ API is available
        return nullptr;
    }

    bool IRBlock::IsValid() const {
        return impl_->block != nullptr;
    }

}
