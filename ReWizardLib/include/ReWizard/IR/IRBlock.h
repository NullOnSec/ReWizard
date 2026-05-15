#ifndef IR_BLOCK_H
#define IR_BLOCK_H

#include <ReWizard/IR/LLVMForwardDecl.h>
#include <cstdint>
#include <memory>
#include <string>

namespace ReWizard {

    // Wraps an llvm::BasicBlock for ReWizard integration.
    // Provides a stable C++ interface that doesn't expose LLVM headers to consumers.
    class IRBlock {
    public:
        IRBlock();
        ~IRBlock();

        // Non-copyable
        IRBlock(const IRBlock&) = delete;
        IRBlock& operator=(const IRBlock&) = delete;

        // Movable
        IRBlock(IRBlock&&) noexcept;
        IRBlock& operator=(IRBlock&&) noexcept;

        // Create from an existing llvm::BasicBlock (takes ownership of reference)
        static std::unique_ptr<IRBlock> Wrap(llvm::BasicBlock* bb);

        // Access the underlying llvm::BasicBlock (for pass code that needs LLVM API)
        llvm::BasicBlock* GetLLVMBlock() const;

        // Address mapping: the native address this block was lifted from
        uintptr_t GetNativeAddress() const { return nativeAddr_; }
        void SetNativeAddress(uintptr_t addr) { nativeAddr_ = addr; }

        // Name for debugging
        const std::string& GetName() const { return name_; }
        void SetName(const std::string& name) { name_ = name; }

        // Whether this block has been lifted successfully
        bool IsValid() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
        uintptr_t nativeAddr_ = 0;
        std::string name_;
    };

}

#endif
