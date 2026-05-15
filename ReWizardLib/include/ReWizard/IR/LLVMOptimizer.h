#ifndef LLVM_OPTIMIZER_H
#define LLVM_OPTIMIZER_H

#include <ReWizard/IR/LLVMForwardDecl.h>
#include <string>

namespace ReWizard {

    // Runs standard LLVM optimization passes on a lifted module.
    // This is the primary deobfuscation driver: LLVM's powerful optimizer
    // pipeline (SCCP, DCE, GVN, SimplifyCFG, etc.) does the heavy lifting
    // of simplifying obfuscated code when presented as IR.
    class LLVMOptimizer {
    public:
        enum class Level {
            O0, // No optimizations
            O1, // Basic cleanup
            O2, // Standard optimizations (recommended for deobfuscation)
            O3, // Aggressive optimizations
        };

        // Run the specified optimization level on the module.
        // Returns true on success.
        static bool Run(llvm::Module* module, Level level = Level::O2);

        // Dump the module IR to a string.
        static std::string DumpIR(llvm::Module* module);
    };

}

#endif
