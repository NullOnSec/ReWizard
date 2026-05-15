cmake_minimum_required(VERSION 3.15)

set(BOOST_INSTALL CACHE PATH "Path to Boost install root")
if(NOT BOOST_INSTALL)
    message(FATAL_ERROR "BOOST_INSTALL not set! Pass -DBOOST_INSTALL=/path/to/boost")
endif()

# Make sure CMake can find BoostConfig.cmake
list(PREPEND CMAKE_PREFIX_PATH "${BOOST_INSTALL}/lib/cmake/Boost-1.85.0")

project("ReWizard")

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

find_package(Boost REQUIRED)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

add_subdirectory("thirdparty")
add_subdirectory("ReWizardLib")
add_subdirectory("ReWizardCLI")

enable_testing()
add_subdirectory("tests")
