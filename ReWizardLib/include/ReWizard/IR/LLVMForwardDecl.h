#ifndef LLVM_FORWARD_DECL_H
#define LLVM_FORWARD_DECL_H

// Forward declarations for LLVM C++ API types
// Consumers include this instead of heavy LLVM headers

namespace llvm {
    class LLVMContext;
    class Module;
    class Function;
    class BasicBlock;
    class Value;
    class Instruction;
    class Type;
    class IntegerType;
    class PointerType;
    class FunctionType;
    class MemoryBuffer;
}

#endif
