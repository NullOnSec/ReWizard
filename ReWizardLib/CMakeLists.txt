set(PASS_SOURCES 
    "source/Analysis/Passes/StaticControlFlowRebuilder.cpp"    
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
    "source/Analysis/PassProvider.cpp"
    ${PASS_SOURCES} # All passes included here
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardLib PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardLib PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${UNICORN_INCLUDE_DIR}"
)

target_link_libraries(ReWizardLib PRIVATE LIEF::LIEF lief_spdlog Zydis Boost::headers unicorn)

# Ensure all passes are linked in (WHOLEARCHIVE)
if (MSVC)
    # MSVC: /WHOLEARCHIVE applies per object file or library
    foreach(pass_file ${PASS_SOURCES})
        get_filename_component(pass_obj ${pass_file} NAME_WE)
        target_link_options(ReWizardLib PRIVATE "/WHOLEARCHIVE:${pass_obj}.obj")
    endforeach()
else()
    # GCC/Clang: wrap entire lib in whole-archive
    target_link_options(ReWizardLib PRIVATE "-Wl,--whole-archive")
    target_link_options(ReWizardLib PRIVATE ReWizardLib) # the static lib itself
    target_link_options(ReWizardLib PRIVATE "-Wl,--no-whole-archive")
endif()
