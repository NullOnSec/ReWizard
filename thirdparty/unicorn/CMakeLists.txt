include(FetchContent)

set(UNICORN_BUILD_SHARED OFF CACHE INTERNAL "Build static") 
set(UNICORN_BUILD_TESTS OFF CACHE INTERNAL "Do not build tests") 
set(UNICORN_ARCH "x86" CACHE INTERNAL "Only x86 target for now") 
 

FetchContent_Declare(
  unicorn
  GIT_REPOSITORY https://github.com/unicorn-engine/unicorn.git
  GIT_TAG 2.1.0
  GIT_SHALLOW TRUE
  CMAKE_ARGS ${UNICORN_CMAKE_ARGS}
)

FetchContent_MakeAvailable(unicorn)