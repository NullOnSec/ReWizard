set(PASS_SOURCES 
    "source/Analysis/Passes/StaticControlFlowRebuilder.cpp"
    "source/Analysis/Passes/ImportAnalysisPass.cpp"
    "source/Analysis/Passes/DataFlowAnalysisPass.cpp"
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
    ${PASS_SOURCES} # All passes included here
)

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
