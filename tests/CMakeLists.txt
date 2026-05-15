add_executable(ReWizardTests
    test_disassembler.cpp
    test_module.cpp
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardTests PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardTests PRIVATE
    "${CMAKE_SOURCE_DIR}/ReWizardLib/include"
)

target_link_libraries(ReWizardTests PRIVATE
    ReWizardLib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ReWizardTests)
