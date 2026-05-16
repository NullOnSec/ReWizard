add_executable(ReWizardTests
    test_main.cpp
    test_disassembler.cpp
    test_module.cpp
    test_symbol_table.cpp
    test_import_analysis.cpp
    test_cfg.cpp
    test_hybrid_analysis.cpp
    test_opaque_predicate.cpp
    test_ir_lifter.cpp
    test_database.cpp
    test_xref_manager.cpp
    test_symbol_manager.cpp
    test_bochs_init.cpp
    test_bochs_hybrid_e2e.cpp
    test_bochs_executor.cpp
    test_bochs_boot_disk.cpp
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardTests PROPERTY CXX_STANDARD 20)
endif()

set(BOCHS_SRC_DIR "Z:/bochs/bochs-3.0-msvc-src/bochs-3.0")
target_include_directories(ReWizardTests PRIVATE
    "${CMAKE_SOURCE_DIR}/ReWizardLib/include"
    "${BOCHS_INSTALL_DIR}/include/bochs"
    "${BOCHS_SRC_DIR}"
)

# We link our own test_main.cpp (provides main()) + GTest::gtest (no main).
# BochsRuntime contains main.obj which also defines main().
# MSVC: two object files defining main() causes LNK2005.
# /FORCE:MULTIPLE makes the linker keep the first definition (our test_main.cpp)
# and ignore the second (Bochs main.obj).
target_link_libraries(ReWizardTests PRIVATE
    GTest::gtest
    ReWizardLib
    BochsRuntime
)
if (MSVC)
    target_link_options(ReWizardTests PRIVATE /FORCE:MULTIPLE)
endif()

include(GoogleTest)
gtest_discover_tests(ReWizardTests)
