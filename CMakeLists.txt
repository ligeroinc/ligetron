cmake_minimum_required(VERSION 3.14)

project(LigeroVM C CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(OpenSSL REQUIRED)
find_package(Sodium  REQUIRED)
find_package(GMP     REQUIRED)
find_package(HEXL    REQUIRED)
find_package(Boost
  COMPONENTS random log exception unit_test_framework
  REQUIRED)

# add_compile_options(--target=wasm32-unknown-unknown-wasm -c -Wl,--allow-undefined)
add_compile_options(-std=c++20 -O0 -g)
# add_compile_options(-sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH -fno-exceptions -mbulk-memory -msimd128 -march=native)
# add_link_options(-static)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/depends/wabt/src)
include_directories(${PROJECT_SOURCE_DIR}/depends/wabt)
include_directories(${HEXL_INCLUDE_DIR})

add_subdirectory(depends/wabt)

## include wabt's generated config file
include_directories(${WABT_BINARY_DIR})

macro (link_binaries)
  foreach(BINARY IN ITEMS ${ARGN})
	target_link_libraries(${BINARY} ${Boost_LIBRARIES} GMP::gmpxx_static HEXL::hexl OpenSSL::SSL OpenSSL::Crypto Sodium::sodium_static wabt tbb)
  endforeach()
endmacro()

# add_library(fib.wasm OBJECT src/fib.c)
# add_executable(LR src/LR.cpp)
add_executable(wt src/wasm.cpp)
link_binaries(wt )
