#include <ReWizard/IR/IRBlock.h>

#ifdef REWIZARD_LLVM_ENABLED
#include <llvm/IR/BasicBlock.h>
#endif

namespace ReWizard {

    class IRBlock::Impl {
    public:
#ifdef REWIZARD_LLVM_ENABLED
        llvm::BasicBlock* block = nullptr;
#else
        void* block = nullptr;
#endif
    };

    IRBlock::IRBlock() : impl_(std::make_unique<Impl>()) {}

    IRBlock::~IRBlock() = default;

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
        auto result = std::make_unique<IRBlock>();
        if (bb) {
            result->SetName(bb->getName().str());
#ifdef REWIZARD_LLVM_ENABLED
            result->impl_->block = bb;
#endif
        }
        return result;
    }

    llvm::BasicBlock* IRBlock::GetLLVMBlock() const {
#ifdef REWIZARD_LLVM_ENABLED
        return impl_->block;
#else
        return nullptr;
#endif
    }

    bool IRBlock::IsValid() const {
#ifdef REWIZARD_LLVM_ENABLED
        return impl_->block != nullptr;
#else
        return false;
#endif
    }

}
