set(PASS_SOURCES 
    "source/Analysis/Passes/StaticControlFlowRebuilder.cpp"
    "source/Analysis/Passes/ImportAnalysisPass.cpp"
    "source/Analysis/Passes/DataFlowAnalysisPass.cpp"
    "source/Analysis/Passes/AbstractInterpretationPass.cpp"
    "source/Analysis/Passes/HybridAnalysisPass.cpp"
    "source/Analysis/Passes/OpaquePredicatePass.cpp"
)

set(IR_SOURCES
    "source/IR/Lifter.cpp"
    "source/IR/IRBlock.cpp"
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
    "source/Analysis/PassProvider.cpp"
    "source/Analysis/AnalysisResult.cpp"
    "source/Hybrid/SimpleTraceReader.cpp"
    "source/Memory/MemoryMapper.cpp"
    "source/Memory/Win32MemoryMapper.cpp"
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

if (EXISTS "Z:/llvm/lib/cmake/llvm")
    list(APPEND CMAKE_PREFIX_PATH "Z:/llvm/lib/cmake/llvm")
endif()

find_package(LLVM CONFIG QUIET)
if (LLVM_FOUND)
    message(STATUS "LLVM found: ${LLVM_PACKAGE_VERSION}")
    target_compile_definitions(ReWizardLib PUBLIC REWIZARD_LLVM_ENABLED)
    target_include_directories(ReWizardLib SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(ReWizardLib PUBLIC ${LLVM_LIBRARIES})
else()
    message(STATUS "LLVM not found, using source headers only")
    if (EXISTS "Z:/llvm-src/llvm/include")
        target_include_directories(ReWizardLib SYSTEM PUBLIC "Z:/llvm-src/llvm/include")
    endif()
endif()

# TODO: Link LLVM libraries when built for correct architecture
# Currently LLVM-C.lib is x64 but target is x86
# Build LLVM from source with matching architecture, then link here

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardLib PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardLib PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${UNICORN_INCLUDE_DIR}"
)

target_link_libraries(ReWizardLib PUBLIC LIEF::LIEF lief_spdlog Zydis Boost::headers unicorn nlohmann_json::nlohmann_json)

# Note: Pass auto-registration happens via inline statics in headers included by
# PassProvider.cpp, so /WHOLEARCHIVE is unnecessary for the current mechanism.
