#ifndef LIFTER_H
#define LIFTER_H

#include <ReWizard/IR/LLVMForwardDecl.h>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace ReWizard {

    // Lifts raw x86/x86_64 binary bytes to LLVM IR.
    // The implementation uses remill (Trail of Bits) when available,
    // or falls back to a manual LLVM C API-based lifter for simple instructions.
    class Lifter {
    public:
        struct Options {
            uintptr_t baseAddress = 0;       // Virtual address where bytes are loaded
            bool is64Bit = true;             // true = x86_64, false = x86
            bool liftMemoryAccesses = true;  // Include memory load/store in IR
        };

        struct LiftResult {
            llvm::Function* function = nullptr;   // Owning function (lives in Lifter's module)
            llvm::BasicBlock* entryBlock = nullptr;
            size_t bytesConsumed = 0;             // How many input bytes were lifted
            bool success = false;
            std::string errorMessage;
        };

        Lifter();
        explicit Lifter(bool is64Bit);
        ~Lifter();

        // Non-copyable, movable
        Lifter(const Lifter&) = delete;
        Lifter& operator=(const Lifter&) = delete;
        Lifter(Lifter&&) noexcept;
        Lifter& operator=(Lifter&&) noexcept;

        // Lift a single basic block starting at 'address'
        // 'bytes' contains the raw machine code starting at 'address'
        LiftResult LiftBasicBlock(const uint8_t* bytes, size_t len, const Options& opts);

        // Lift an entire function (sequence of basic blocks) starting at 'address'
        // This requires CFG information from StaticControlFlowRebuilder
        LiftResult LiftFunction(const uint8_t* bytes, size_t len, const Options& opts);

        // Get the LLVM module that owns all lifted functions
        llvm::Module* GetModule() const;

        // Get the LLVM context
        llvm::LLVMContext* GetContext() const;

        // Dump the entire module as LLVM IR text
        std::string DumpModule() const;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}

#endif
