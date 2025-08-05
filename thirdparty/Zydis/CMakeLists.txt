include(FetchContent)

set(ZYDIS_CMAKE_ARGS
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DZYDIS_BUILD_EXAMPLES=OFF
  -DZYDIS_BUILD_DOXYGEN=OFF

)

FetchContent_Declare(
  zydis
  GIT_REPOSITORY https://github.com/zyantific/zydis.git
  GIT_TAG v4.1.0
  GIT_SHALLOW TRUE
  CMAKE_ARGS ${ZYDIS_CMAKE_ARGS}
)

FetchContent_MakeAvailable(zydis)