#include <ReWizard/IR/Lifter.h>

#include <spdlog/spdlog.h>
#include <sstream>

#ifdef REWIZARD_REMILL_ENABLED
#include <remill/Arch/Arch.h>
#include <remill/Arch/Instruction.h>
#include <remill/BC/ABI.h>
#include <remill/BC/IntrinsicTable.h>
#include <remill/BC/InstructionLifter.h>
#include <remill/BC/Util.h>
#endif

#ifdef REWIZARD_LLVM_ENABLED
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#endif

namespace ReWizard {

#ifdef REWIZARD_REMILL_ENABLED

    class Lifter::Impl {
    public:
        std::unique_ptr<llvm::LLVMContext> ownedContext;
        std::unique_ptr<llvm::Module> ownedModule;
        remill::Arch::ArchPtr arch;
        std::unique_ptr<remill::IntrinsicTable> intrinsics;
        std::shared_ptr<remill::InstructionLifterIntf> inst_lifter;

        Impl() : Impl(true) {}

        explicit Impl(bool is64Bit) {
            ownedContext = std::make_unique<llvm::LLVMContext>();

            std::string arch_name = is64Bit ? "amd64" : "x86";
            std::string os_name = "windows";
            arch = remill::Arch::Get(*ownedContext, os_name, arch_name);

            std::vector<std::filesystem::path> semantics_dirs;
            semantics_dirs.emplace_back("Z:/remill/build/lib/Arch/X86/Runtime");
            ownedModule = remill::LoadArchSemantics(arch.get(), semantics_dirs);
            if (!ownedModule) {
                spdlog::error("Lifter: failed to load remill semantics for {}", arch_name);
                ownedModule = std::make_unique<llvm::Module>("rewizard_lift", *ownedContext);
            }

            arch->PrepareModule(ownedModule.get());
            ownedModule->setModuleIdentifier("rewizard_lift");
            ownedModule->setSourceFileName("rewizard_lift");

            intrinsics = std::make_unique<remill::IntrinsicTable>(ownedModule.get());
            inst_lifter = std::dynamic_pointer_cast<remill::InstructionLifterIntf>(
                arch->DefaultLifter(*intrinsics));
        }

        llvm::LLVMContext& Context() { return *ownedContext; }
        llvm::Module& Module() { return *ownedModule; }
    };

#elif defined(REWIZARD_LLVM_ENABLED)

    class Lifter::Impl {
    public:
        std::unique_ptr<llvm::LLVMContext> ownedContext;
        std::unique_ptr<llvm::Module> ownedModule;

        Impl() : Impl(true) {}

        explicit Impl(bool is64Bit) {
            ownedContext = std::make_unique<llvm::LLVMContext>();
            ownedModule = std::make_unique<llvm::Module>("rewizard_lift", *ownedContext);
            SetDefaults(*ownedModule, is64Bit);
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

        llvm::LLVMContext& Context() { return *ownedContext; }
        llvm::Module& Module() { return *ownedModule; }
    };

#else

    class Lifter::Impl {
    public:
        Impl() {}
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

#if defined(REWIZARD_REMILL_ENABLED)

        auto& mod = impl_->Module();
        auto& arch = *impl_->arch;
        auto& intrinsics = *impl_->intrinsics;
        auto& inst_lifter = *impl_->inst_lifter;

        std::stringstream funcNameStream;
        funcNameStream << "bb_" << std::hex << opts.baseAddress;
        std::string funcName = funcNameStream.str();

        auto* func = arch.DefineLiftedFunction(funcName, &mod);
        if (!func) {
            result.errorMessage = "Failed to define lifted function";
            return result;
        }

        func->setLinkage(llvm::Function::InternalLinkage);

        auto* block = &func->getEntryBlock();
        auto* state_ptr = remill::LoadStatePointer(func);

        for (auto& inst : *block) {
            if (auto* store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
                if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(store->getPointerOperand())) {
                    if (alloca->getName() == "NEXT_PC") {
                        auto* addr_val = llvm::ConstantInt::get(
                            alloca->getAllocatedType(),
                            opts.baseAddress, false);
                        store->setOperand(0, addr_val);
                        break;
                    }
                }
            }
        }

        size_t offset = 0;
        auto decoding_context = arch.CreateInitialContext();

        while (offset < len) {
            remill::Instruction inst;
            std::string_view byte_view(
                reinterpret_cast<const char*>(bytes + offset),
                len - offset);

            if (!arch.DecodeInstruction(
                    opts.baseAddress + offset, byte_view,
                    inst, decoding_context)) {
                result.errorMessage = "Failed to decode instruction at offset " +
                                      std::to_string(offset);
                return result;
            }

            auto status = inst_lifter.LiftIntoBlock(inst, block, state_ptr);
            if (status != remill::kLiftedInstruction) {
                if (status == remill::kLiftedUnsupportedInstruction) {
                    spdlog::warn("Lifter: unsupported instruction at 0x{:x}, skipping",
                                 opts.baseAddress + offset);
                } else {
                    result.errorMessage = "Failed to lift instruction at offset " +
                                          std::to_string(offset);
                    return result;
                }
            }

            offset += inst.NumBytes();

            if (inst.IsControlFlow()) {
                break;
            }
        }

        auto* mem_ptr = remill::LoadMemoryPointer(block, intrinsics);
        llvm::IRBuilder<> builder(block);
        builder.CreateRet(mem_ptr);

        result.success = true;
        result.bytesConsumed = offset;
        result.function = func;
        result.entryBlock = block;

        spdlog::debug("Lifter: lifted basic block at 0x{:x} ({} bytes)",
                      opts.baseAddress, offset);
        return result;

#elif defined(REWIZARD_LLVM_ENABLED)
        result.errorMessage = "remill not available; binary lifting disabled";
        spdlog::error("Lifter: remill not available; cannot lift basic block at 0x{:x}", opts.baseAddress);
        return result;
#else
        result.errorMessage = "LLVM not enabled";
        spdlog::warn("Lifter: LLVM not enabled, stub lift of basic block at 0x{:x} ({} bytes)", opts.baseAddress, len);
        return result;
#endif
    }

    Lifter::LiftResult Lifter::LiftFunction(const uint8_t* bytes, size_t len, const Options& opts) {
        return LiftBasicBlock(bytes, len, opts);
    }

    llvm::Module* Lifter::GetModule() const {
#if defined(REWIZARD_LLVM_ENABLED)
        return &impl_->Module();
#else
        return nullptr;
#endif
    }

    llvm::LLVMContext* Lifter::GetContext() const {
#if defined(REWIZARD_LLVM_ENABLED)
        return &impl_->Context();
#else
        return nullptr;
#endif
    }

    std::string Lifter::DumpModule() const {
#if defined(REWIZARD_LLVM_ENABLED)
        std::string str;
        llvm::raw_string_ostream os(str);
        impl_->Module().print(os, nullptr);
        return str;
#else
        return "; ModuleID = 'rewizard_lift'\nsource_filename = \"rewizard_lift\"\n";
#endif
    }

}
