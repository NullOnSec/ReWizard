#include <ReWizard/IR/Lifter.h>
#include <ReWizard/Disassembler/Disassembler.h>

#include <spdlog/spdlog.h>
#include <sstream>

#ifdef REWIZARD_LLVM_ENABLED
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>
#endif

namespace ReWizard {

#ifdef REWIZARD_LLVM_ENABLED

    class Lifter::Impl {
    public:
        std::unique_ptr<llvm::LLVMContext> ownedContext;
        std::unique_ptr<llvm::Module> ownedModule;
        llvm::LLVMContext& context;
        llvm::Module& module;

        Impl()
            : ownedContext(std::make_unique<llvm::LLVMContext>()),
              ownedModule(std::make_unique<llvm::Module>("rewizard_lift", *ownedContext)),
              context(*ownedContext),
              module(*ownedModule) {
            SetDefaults(module, true);
        }

        explicit Impl(bool is64Bit)
            : ownedContext(std::make_unique<llvm::LLVMContext>()),
              ownedModule(std::make_unique<llvm::Module>("rewizard_lift", *ownedContext)),
              context(*ownedContext),
              module(*ownedModule) {
            SetDefaults(module, is64Bit);
        }

        static void SetDefaults(llvm::Module& mod, bool is64Bit) {
            if (is64Bit) {
                mod.setDataLayout("e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128");
                mod.setTargetTriple("x86_64-pc-windows-msvc");
            } else {
                mod.setDataLayout("e-m:e-p:32:32-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128");
                mod.setTargetTriple("i686-pc-windows-msvc");
            }
        }
    };

    static int GetRegisterIndex(ZydisRegister reg) {
        switch (reg) {
        case ZYDIS_REGISTER_RAX: case ZYDIS_REGISTER_EAX: case ZYDIS_REGISTER_AX: case ZYDIS_REGISTER_AL: case ZYDIS_REGISTER_AH: return 0;
        case ZYDIS_REGISTER_RCX: case ZYDIS_REGISTER_ECX: case ZYDIS_REGISTER_CX: case ZYDIS_REGISTER_CL: case ZYDIS_REGISTER_CH: return 1;
        case ZYDIS_REGISTER_RDX: case ZYDIS_REGISTER_EDX: case ZYDIS_REGISTER_DX: case ZYDIS_REGISTER_DL: case ZYDIS_REGISTER_DH: return 2;
        case ZYDIS_REGISTER_RBX: case ZYDIS_REGISTER_EBX: case ZYDIS_REGISTER_BX: case ZYDIS_REGISTER_BL: case ZYDIS_REGISTER_BH: return 3;
        case ZYDIS_REGISTER_RSP: case ZYDIS_REGISTER_ESP: case ZYDIS_REGISTER_SP: case ZYDIS_REGISTER_SPL: return 4;
        case ZYDIS_REGISTER_RBP: case ZYDIS_REGISTER_EBP: case ZYDIS_REGISTER_BP: case ZYDIS_REGISTER_BPL: return 5;
        case ZYDIS_REGISTER_RSI: case ZYDIS_REGISTER_ESI: case ZYDIS_REGISTER_SI: case ZYDIS_REGISTER_SIL: return 6;
        case ZYDIS_REGISTER_RDI: case ZYDIS_REGISTER_EDI: case ZYDIS_REGISTER_DI: case ZYDIS_REGISTER_DIL: return 7;
        case ZYDIS_REGISTER_R8:  case ZYDIS_REGISTER_R8D:  case ZYDIS_REGISTER_R8W:  case ZYDIS_REGISTER_R8B:  return 8;
        case ZYDIS_REGISTER_R9:  case ZYDIS_REGISTER_R9D:  case ZYDIS_REGISTER_R9W:  case ZYDIS_REGISTER_R9B:  return 9;
        case ZYDIS_REGISTER_R10: case ZYDIS_REGISTER_R10D: case ZYDIS_REGISTER_R10W: case ZYDIS_REGISTER_R10B: return 10;
        case ZYDIS_REGISTER_R11: case ZYDIS_REGISTER_R11D: case ZYDIS_REGISTER_R11W: case ZYDIS_REGISTER_R11B: return 11;
        case ZYDIS_REGISTER_R12: case ZYDIS_REGISTER_R12D: case ZYDIS_REGISTER_R12W: case ZYDIS_REGISTER_R12B: return 12;
        case ZYDIS_REGISTER_R13: case ZYDIS_REGISTER_R13D: case ZYDIS_REGISTER_R13W: case ZYDIS_REGISTER_R13B: return 13;
        case ZYDIS_REGISTER_R14: case ZYDIS_REGISTER_R14D: case ZYDIS_REGISTER_R14W: case ZYDIS_REGISTER_R14B: return 14;
        case ZYDIS_REGISTER_R15: case ZYDIS_REGISTER_R15D: case ZYDIS_REGISTER_R15W: case ZYDIS_REGISTER_R15B: return 15;
        default: return -1;
        }
    }

    static int GetRegisterSizeBits(ZydisRegister reg) {
        switch (reg) {
        case ZYDIS_REGISTER_AL: case ZYDIS_REGISTER_AH: case ZYDIS_REGISTER_BL: case ZYDIS_REGISTER_BH:
        case ZYDIS_REGISTER_CL: case ZYDIS_REGISTER_CH: case ZYDIS_REGISTER_DL: case ZYDIS_REGISTER_DH:
        case ZYDIS_REGISTER_SPL: case ZYDIS_REGISTER_BPL: case ZYDIS_REGISTER_SIL: case ZYDIS_REGISTER_DIL:
        case ZYDIS_REGISTER_R8B: case ZYDIS_REGISTER_R9B: case ZYDIS_REGISTER_R10B: case ZYDIS_REGISTER_R11B:
        case ZYDIS_REGISTER_R12B: case ZYDIS_REGISTER_R13B: case ZYDIS_REGISTER_R14B: case ZYDIS_REGISTER_R15B:
            return 8;
        case ZYDIS_REGISTER_AX: case ZYDIS_REGISTER_CX: case ZYDIS_REGISTER_DX: case ZYDIS_REGISTER_BX:
        case ZYDIS_REGISTER_SP: case ZYDIS_REGISTER_BP: case ZYDIS_REGISTER_SI: case ZYDIS_REGISTER_DI:
        case ZYDIS_REGISTER_R8W: case ZYDIS_REGISTER_R9W: case ZYDIS_REGISTER_R10W: case ZYDIS_REGISTER_R11W:
        case ZYDIS_REGISTER_R12W: case ZYDIS_REGISTER_R13W: case ZYDIS_REGISTER_R14W: case ZYDIS_REGISTER_R15W:
            return 16;
        case ZYDIS_REGISTER_EAX: case ZYDIS_REGISTER_ECX: case ZYDIS_REGISTER_EDX: case ZYDIS_REGISTER_EBX:
        case ZYDIS_REGISTER_ESP: case ZYDIS_REGISTER_EBP: case ZYDIS_REGISTER_ESI: case ZYDIS_REGISTER_EDI:
        case ZYDIS_REGISTER_R8D: case ZYDIS_REGISTER_R9D: case ZYDIS_REGISTER_R10D: case ZYDIS_REGISTER_R11D:
        case ZYDIS_REGISTER_R12D: case ZYDIS_REGISTER_R13D: case ZYDIS_REGISTER_R14D: case ZYDIS_REGISTER_R15D:
            return 32;
        default:
            return 64;
        }
    }

#else // !REWIZARD_LLVM_ENABLED

    class Lifter::Impl {
    public:
        Impl() {}
        ~Impl() {}
    };

#endif

    Lifter::Lifter() : impl_(std::make_unique<Impl>()) {}

    Lifter::Lifter(bool is64Bit) : impl_(std::make_unique<Impl>(is64Bit)) {}

    Lifter::~Lifter() = default;

    Lifter::Lifter(Lifter&& other) noexcept = default;
    Lifter& Lifter::operator=(Lifter&& other) noexcept = default;

    Lifter::LiftResult Lifter::LiftBasicBlock(const uint8_t* bytes, size_t len, const Options& opts) {
        LiftResult result;

        if (!bytes || len == 0) {
            result.errorMessage = "Empty input bytes";
            return result;
        }

#ifndef REWIZARD_LLVM_ENABLED
        result.success = true;
        result.bytesConsumed = len;
        spdlog::debug("Lifter: LLVM not enabled, stub lift of basic block at 0x{:x} ({} bytes)", opts.baseAddress, len);
        return result;
#else
        auto& ctx = impl_->context;
        auto& mod = impl_->module;

        auto* int64Ty = llvm::Type::getInt64Ty(ctx);
        auto* int32Ty = llvm::Type::getInt32Ty(ctx);
        auto* int8Ty = llvm::Type::getInt8Ty(ctx);
        auto* voidTy = llvm::Type::getVoidTy(ctx);

        std::stringstream funcNameStream;
        funcNameStream << "bb_" << std::hex << opts.baseAddress;
        std::string funcName = funcNameStream.str();
        auto* funcTy = llvm::FunctionType::get(voidTy, false);
        auto* func = llvm::Function::Create(funcTy, llvm::Function::InternalLinkage, funcName, mod);
        auto* entry = llvm::BasicBlock::Create(ctx, "entry", func);
        llvm::IRBuilder<> builder(entry);

        auto* regArrayTy = llvm::ArrayType::get(int64Ty, 16);
        auto* regs = builder.CreateAlloca(regArrayTy, nullptr, "regs");

        auto getRegPtr = [&](int idx) -> llvm::Value* {
            auto* zero = builder.getInt32(0);
            auto* idxVal = builder.getInt32(idx);
            return builder.CreateGEP(regArrayTy, regs, { zero, idxVal });
        };

        auto loadReg = [&](int idx) -> llvm::Value* {
            return builder.CreateLoad(int64Ty, getRegPtr(idx), "r" + std::to_string(idx));
        };

        auto storeReg = [&](int idx, llvm::Value* val) {
            builder.CreateStore(val, getRegPtr(idx));
        };

        size_t offset = 0;
        auto mode = opts.is64Bit ? ZYDIS_MACHINE_MODE_LONG_64 : ZYDIS_MACHINE_MODE_LEGACY_32;
        auto stackWidth = opts.is64Bit ? ZYDIS_STACK_WIDTH_64 : ZYDIS_STACK_WIDTH_32;
        auto& dis = Disassembler::Get(mode, stackWidth);

        while (offset < len) {
            auto insn = dis->DisassembleSingle<DecodedInstruction>(
                const_cast<uint8_t*>(bytes + offset),
                len - offset
            );
            if (!insn) {
                result.errorMessage = "Failed to decode instruction at offset " + std::to_string(offset);
                return result;
            }

            const auto& zydisInsn = insn->Instruction();
            size_t insnLen = zydisInsn.length;
            uintptr_t insnAddr = opts.baseAddress + offset;

            bool isTerminator = false;

            switch (zydisInsn.mnemonic) {
            case ZYDIS_MNEMONIC_NOP:
                break;

            case ZYDIS_MNEMONIC_MOV: {
                if (zydisInsn.operand_count_visible >= 2) {
                    const auto& dst = insn->Operands()[0];
                    const auto& src = insn->Operands()[1];

                    if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                        int dstIdx = GetRegisterIndex(dst.reg.value);
                        if (dstIdx >= 0) {
                            int sizeBits = GetRegisterSizeBits(dst.reg.value);
                            llvm::Value* val = nullptr;
                            if (sizeBits == 64) {
                                val = builder.getInt64(src.imm.value.u);
                            } else if (sizeBits == 32) {
                                val = builder.getInt64(static_cast<uint32_t>(src.imm.value.u));
                            } else if (sizeBits == 16) {
                                val = builder.getInt64(static_cast<uint16_t>(src.imm.value.u));
                            } else {
                                val = builder.getInt64(static_cast<uint8_t>(src.imm.value.u));
                            }
                            storeReg(dstIdx, val);
                        }
                    } else if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                        int dstIdx = GetRegisterIndex(dst.reg.value);
                        int srcIdx = GetRegisterIndex(src.reg.value);
                        if (dstIdx >= 0 && srcIdx >= 0) {
                            storeReg(dstIdx, loadReg(srcIdx));
                        }
                    }
                }
                break;
            }

            case ZYDIS_MNEMONIC_ADD: {
                if (zydisInsn.operand_count_visible >= 2) {
                    const auto& dst = insn->Operands()[0];
                    const auto& src = insn->Operands()[1];
                    if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                        int dstIdx = GetRegisterIndex(dst.reg.value);
                        if (dstIdx >= 0) {
                            auto* cur = loadReg(dstIdx);
                            auto* addend = builder.getInt64(src.imm.value.u);
                            storeReg(dstIdx, builder.CreateAdd(cur, addend, "add"));
                        }
                    }
                }
                break;
            }

            case ZYDIS_MNEMONIC_SUB: {
                if (zydisInsn.operand_count_visible >= 2) {
                    const auto& dst = insn->Operands()[0];
                    const auto& src = insn->Operands()[1];
                    if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                        int dstIdx = GetRegisterIndex(dst.reg.value);
                        if (dstIdx >= 0) {
                            auto* cur = loadReg(dstIdx);
                            auto* subtrahend = builder.getInt64(src.imm.value.u);
                            storeReg(dstIdx, builder.CreateSub(cur, subtrahend, "sub"));
                        }
                    }
                }
                break;
            }

            case ZYDIS_MNEMONIC_XOR: {
                if (zydisInsn.operand_count_visible >= 2) {
                    const auto& dst = insn->Operands()[0];
                    const auto& src = insn->Operands()[1];
                    if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_REGISTER) {
                        int dstIdx = GetRegisterIndex(dst.reg.value);
                        int srcIdx = GetRegisterIndex(src.reg.value);
                        if (dstIdx >= 0 && srcIdx >= 0 && dstIdx == srcIdx) {
                            storeReg(dstIdx, builder.getInt64(0));
                        }
                    } else if (dst.type == ZYDIS_OPERAND_TYPE_REGISTER && src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                        int dstIdx = GetRegisterIndex(dst.reg.value);
                        if (dstIdx >= 0) {
                            auto* cur = loadReg(dstIdx);
                            auto* imm = builder.getInt64(src.imm.value.u);
                            storeReg(dstIdx, builder.CreateXor(cur, imm, "xor"));
                        }
                    }
                }
                break;
            }

            case ZYDIS_MNEMONIC_RET: {
                builder.CreateRetVoid();
                isTerminator = true;
                break;
            }

            default:
                spdlog::debug("Lifter: unhandled mnemonic {}, treating as nop", static_cast<int>(zydisInsn.mnemonic));
                break;
            }

            offset += insnLen;

            if (isTerminator) {
                break;
            }

            if (offset >= len && !isTerminator) {
                builder.CreateRetVoid();
                break;
            }
        }

        result.success = true;
        result.bytesConsumed = offset;
        result.function = func;
        result.entryBlock = entry;

        spdlog::debug("Lifter: lifted basic block at 0x{:x} ({} bytes, {} instructions)",
                      opts.baseAddress, offset, func->size());
        return result;
#endif
    }

    Lifter::LiftResult Lifter::LiftFunction(const uint8_t* bytes, size_t len, const Options& opts) {
        return LiftBasicBlock(bytes, len, opts);
    }

    llvm::Module* Lifter::GetModule() const {
#ifdef REWIZARD_LLVM_ENABLED
        return &impl_->module;
#else
        return nullptr;
#endif
    }

    llvm::LLVMContext* Lifter::GetContext() const {
#ifdef REWIZARD_LLVM_ENABLED
        return &impl_->context;
#else
        return nullptr;
#endif
    }

    std::string Lifter::DumpModule() const {
#ifdef REWIZARD_LLVM_ENABLED
        std::string str;
        llvm::raw_string_ostream os(str);
        impl_->module.print(os, nullptr);
        return str;
#else
        return "; ModuleID = 'rewizard_lift'\nsource_filename = \"rewizard_lift\"\n";
#endif
    }

}
