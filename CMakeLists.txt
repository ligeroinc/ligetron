cmake_minimum_required(VERSION 3.14)

project(LigeroVM C CXX)

add_subdirectory(depends/wabt)

# add_compile_options(--target=wasm32-unknown-unknown-wasm -c -Wl,--allow-undefined)
add_compile_options(-std=c++20 -O0 -g -static)
# add_compile_options(-sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH -fno-exceptions -mbulk-memory -msimd128 -march=native)
add_link_options(-static)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/depends/wabt/src)
include_directories(${PROJECT_SOURCE_DIR}/depends/wabt)

## include wabt's generated config file
include_directories(${WABT_BINARY_DIR})

# add_library(fib.wasm OBJECT src/fib.c)
# add_executable(LR src/LR.cpp)
add_executable(wt src/wasm.cpp)
target_link_libraries(wt wabt)
