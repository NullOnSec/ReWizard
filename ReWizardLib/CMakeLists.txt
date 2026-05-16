set(PASS_SOURCES 
    "source/Analysis/Passes/StaticControlFlowRebuilder.cpp"
    "source/Analysis/Passes/ImportAnalysisPass.cpp"
    "source/Analysis/Passes/DataFlowAnalysisPass.cpp"
    "source/Analysis/Passes/AbstractInterpretationPass.cpp"
    "source/Analysis/Passes/HybridAnalysisPass.cpp"
    "source/Analysis/Passes/OpaquePredicatePass.cpp"
    "source/Analysis/Passes/IRLiftingPass.cpp"
    "source/Analysis/Passes/ConstantFoldingPass.cpp"
)

set(IR_SOURCES
    "source/IR/Lifter.cpp"
    "source/IR/IRBlock.cpp"
    "source/IR/LLVMOptimizer.cpp"
)

add_library(ReWizardLib STATIC
    "source/Disassembler/Disassembler.cpp"
    "source/FileLoader/FileLoader.cpp"
    "source/Analysis/AnalysisContext.cpp"
    "source/Analysis/AnalysisManager.cpp"
    "source/Analysis/PassManager.cpp"
    "source/Analysis/Units/BasicBlock.cpp"
    "source/Analysis/Units/Function.cpp"
    "source/Analysis/Units/Module.cpp"
    "source/Analysis/Units/SymbolTable.cpp"
    "source/Analysis/Units/XrefManager.cpp"
    "source/Analysis/PassProvider.cpp"
    "source/Analysis/AnalysisResult.cpp"
    "source/Hybrid/SimpleTraceReader.cpp"
    "source/Hybrid/BochsExecutor.cpp"
    "source/Hybrid/BochsInstrumentationBridge.cpp"
    "source/Memory/MemoryMapper.cpp"
    "source/Memory/Win32MemoryMapper.cpp"
    "source/Database/AnalysisDatabase.cpp"
    ${PASS_SOURCES} # All passes included here
    ${IR_SOURCES}
)

if (NOT WIN32)
    target_sources(ReWizardLib PRIVATE "source/Memory/PosixMemoryMapper.cpp")
endif()

# Find LLVM
if (EXISTS "$ENV{LLVM_DIR}")
    set(LLVM_DIR "$ENV{LLVM_DIR}" CACHE PATH "Path to LLVM installation")
endif()

if (EXISTS "Z:/llvm-install/lib/cmake/llvm")
    list(APPEND CMAKE_PREFIX_PATH "Z:/llvm-install/lib/cmake/llvm")
elseif (EXISTS "Z:/llvm/lib/cmake/llvm")
    list(APPEND CMAKE_PREFIX_PATH "Z:/llvm/lib/cmake/llvm")
endif()

find_package(LLVM CONFIG QUIET)
if (LLVM_FOUND)
    message(STATUS "LLVM found: ${LLVM_PACKAGE_VERSION}")
    target_compile_definitions(ReWizardLib PUBLIC REWIZARD_LLVM_ENABLED)
    target_include_directories(ReWizardLib SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
    # Map required components to actual library targets
    llvm_map_components_to_libnames(LLVM_REQ_LIBS
        Core Support IRReader X86Info X86Desc X86CodeGen X86AsmParser
        Analysis TransformUtils ScalarOpts InstCombine Passes
    )
    target_link_libraries(ReWizardLib PUBLIC ${LLVM_REQ_LIBS})
else()
    message(WARNING "LLVM C++ libraries not found. IR layer will use forward declarations only.\n"
                    "To enable full LLVM support, build LLVM first:\n"
                    "  .\\scripts\\build-llvm.ps1\n"
                    "Or set LLVM_DIR to an existing installation.")
    if (EXISTS "Z:/llvm-src/llvm/include")
        target_include_directories(ReWizardLib SYSTEM PUBLIC "Z:/llvm-src/llvm/include")
    endif()
endif()

# Find remill (x86 binary lifter)
# We use a manual configuration instead of find_package(remill) because
# remill's cmake install has complex dependencies (sleigh, XED, glog, gflags).
# As long as the libraries and headers are present at Z:/remill-install, we link them directly.
set(REMILL_INSTALL_DIR "Z:/remill-install" CACHE PATH "Path to remill install")
if (EXISTS "${REMILL_INSTALL_DIR}/include/remill/Arch/Arch.h")
    message(STATUS "remill found at ${REMILL_INSTALL_DIR}")
    target_compile_definitions(ReWizardLib PUBLIC REWIZARD_REMILL_ENABLED)
    target_include_directories(ReWizardLib SYSTEM PRIVATE "${REMILL_INSTALL_DIR}/include")
    target_link_libraries(ReWizardLib PUBLIC
        "${REMILL_INSTALL_DIR}/lib/remill_bc.lib"
        "${REMILL_INSTALL_DIR}/lib/remill_os.lib"
        "${REMILL_INSTALL_DIR}/lib/remill_arch.lib"
        "${REMILL_INSTALL_DIR}/lib/remill_arch_x86.lib"
        "${REMILL_INSTALL_DIR}/lib/remill_version.lib"
    )
    # remill depends on XED, glog, gflags which are also in remill-install
    if (EXISTS "${REMILL_INSTALL_DIR}/lib/xed.lib")
        target_link_libraries(ReWizardLib PUBLIC "${REMILL_INSTALL_DIR}/lib/xed.lib")
    endif()
    if (EXISTS "${REMILL_INSTALL_DIR}/lib/glog.lib")
        target_link_libraries(ReWizardLib PUBLIC "${REMILL_INSTALL_DIR}/lib/glog.lib")
    endif()
    if (EXISTS "${REMILL_INSTALL_DIR}/lib/gflags_static.lib")
        target_link_libraries(ReWizardLib PUBLIC "${REMILL_INSTALL_DIR}/lib/gflags_static.lib")
    endif()
    # glog and gflags depend on Windows system libraries
    if (WIN32)
        target_link_libraries(ReWizardLib PUBLIC DbgHelp ShLwApi)
    endif()
else()
    message(WARNING "remill not found at ${REMILL_INSTALL_DIR}. Binary lifting will use the manual Zydis+IRBuilder lifter.\n"
                    "To enable remill-based lifting, build remill first:\n"
                    "  .\\scripts\\build-remill.ps1")
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardLib PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardLib PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Find Bochs (full-system emulation backend)
set(BOCHS_INSTALL_DIR "Z:/bochs-install" CACHE PATH "Path to Bochs install")
if (EXISTS "${BOCHS_INSTALL_DIR}/lib/cpu.lib")
    message(STATUS "Bochs found at ${BOCHS_INSTALL_DIR}")
    target_compile_definitions(ReWizardLib PUBLIC REWIZARD_BOCHS_ENABLED)
    set(BOCHS_SRC_DIR "Z:/bochs/bochs-3.0-msvc-src/bochs-3.0")
    target_include_directories(ReWizardLib SYSTEM PRIVATE
        "${BOCHS_INSTALL_DIR}/include/bochs"
        "${BOCHS_SRC_DIR}"
    )
    # Bochs libraries (device modules, CPU, memory, etc.)
    target_link_libraries(ReWizardLib PUBLIC
        "${BOCHS_INSTALL_DIR}/lib/cpu.lib"
        "${BOCHS_INSTALL_DIR}/lib/avx.lib"
        "${BOCHS_INSTALL_DIR}/lib/memory.lib"
        "${BOCHS_INSTALL_DIR}/lib/fpu.lib"
        "${BOCHS_INSTALL_DIR}/lib/softfloat3e.lib"
        "${BOCHS_INSTALL_DIR}/lib/cpudb.lib"
        "${BOCHS_INSTALL_DIR}/lib/bx_debug.lib"
        "${BOCHS_INSTALL_DIR}/lib/gui.lib"
        "${BOCHS_INSTALL_DIR}/lib/iodev.lib"
        "${BOCHS_INSTALL_DIR}/lib/iodev_display.lib"
        "${BOCHS_INSTALL_DIR}/lib/iodev_hdimage.lib"
        "${BOCHS_INSTALL_DIR}/lib/iodev_network.lib"
        "${BOCHS_INSTALL_DIR}/lib/iodev_sound.lib"
        "${BOCHS_INSTALL_DIR}/lib/iodev_usb.lib"
    )
    # Bochs executable-level object files (globals + init functions)
    # These are not in any .lib; they come from the bochs.exe build.
    # We separate them into a dedicated BochsRuntime library because
    # main.obj defines main() which conflicts with every executable's own main().
    set(BOCHS_SRC_DIR "Z:/bochs/bochs-3.0-msvc-src/bochs-3.0")
    set(BOCHS_OBJ_DIR "${BOCHS_SRC_DIR}/obj-release")
    add_library(BochsRuntime STATIC
        "${BOCHS_OBJ_DIR}/main.obj"
        "${BOCHS_OBJ_DIR}/config.obj"
        "${BOCHS_OBJ_DIR}/plugin.obj"
        "${BOCHS_OBJ_DIR}/pc_system.obj"
        "${BOCHS_OBJ_DIR}/logio.obj"
        "${BOCHS_OBJ_DIR}/osdep.obj"
        "${BOCHS_OBJ_DIR}/bxthread.obj"
        "${BOCHS_OBJ_DIR}/crc.obj"
    )
    set_target_properties(BochsRuntime PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(BochsRuntime PUBLIC
        "${BOCHS_INSTALL_DIR}/include/bochs"
        "${BOCHS_SRC_DIR}"
    )
    # Bochs device libraries depend on Windows system libraries
    if (WIN32)
        target_link_libraries(ReWizardLib PUBLIC comctl32 winmm iphlpapi)
    endif()
else()
    message(WARNING "Bochs not found at ${BOCHS_INSTALL_DIR}. Hybrid analysis will not support full-system emulation.\n"
                    "To enable Bochs emulation, build Bochs first:\n"
                    "  .\scripts\build-bochs.ps1")
endif()

target_link_libraries(ReWizardLib PUBLIC LIEF::LIEF lief_spdlog Zydis Boost::headers nlohmann_json::nlohmann_json sqlite3)

# Note: Pass auto-registration happens via inline statics in headers included by
# PassProvider.cpp, so /WHOLEARCHIVE is unnecessary for the current mechanism.
