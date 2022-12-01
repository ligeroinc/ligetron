cmake_minimum_required(VERSION 3.14)

project(LigeroVM C CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# find_package(Sodium)
# find_package(GMP     REQUIRED)

if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Web")
find_package(OpenSSL REQUIRED)
find_package(HEXL    REQUIRED)
find_package(Boost
  COMPONENTS random log exception serialization
  REQUIRED)
endif()

# set(CMAKE_EXECUTABLE_SUFFIX ".html")


# add_compile_options(--target=wasm32-unknown-unknown-wasm -c -Wl,--allow-undefined)

#-sMODULARIZE=1 -sEXPORT_NAME=createModule

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  add_compile_options(-std=c++20 -Og -g -pg)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Web")
  add_compile_options(-std=c++20 -Wno-narrowing -sUSE_PTHREADS -mbulk-memory -msimd128)
  add_link_options(-sERROR_ON_UNDEFINED_SYMBOLS=0 --preload-file "edit.wasm" -sUSE_PTHREADS -sPTHREAD_POOL_SIZE=2 -sPROXY_TO_PTHREAD=1 -sTOTAL_STACK=8MB -sEXPORTED_RUNTIME_METHODS=FS -sNO_EXIT_RUNTIME=1 -sINITIAL_MEMORY=536870912 --emrun)
  # add_link_options(-sALLOW_MEMORY_GROWTH)
else()
  add_compile_options(-std=c++20 -O3 -march=native -fopenmp)
  # add_link_options(-pg)
  add_link_options(-fopenmp)
endif()

# add_compile_options(-sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH -fno-exceptions -mbulk-memory -msimd128 -march=native)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_PREFIX_PATH}/include)
link_directories(${CMAKE_PREFIX_PATH}/lib)
# include_directories(${Boost_INCLUDE_DIR})
# include_directories(${HEXL_INCLUDE_DIR})

# add_subdirectory(depends/wabt)

## include wabt's generated config file
# include_directories(${WABT_ROOT_PATH}/include)
# include_directories(${WABT_ROOT_PATH}/lib)
# include_directories(${WABT_BINARY_DIR})

# include_directories(${PROJECT_SOURCE_DIR}/depends/wabt/src)
# include_directories(${PROJECT_SOURCE_DIR}/depends/wabt)

macro (link_binaries)
  foreach(BINARY IN ITEMS ${ARGN})
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Web")
	  target_link_libraries(${BINARY} ${Boost_LIBRARIES} hexl ssl crypto wabt boost)
	else()
	  target_link_libraries(${BINARY} ${Boost_LIBRARIES} hexl ssl crypto wabt tbb)
	endif()
  endforeach()
endmacro()

# GMP::gmpxx_static Sodium::sodium_static

# add_library(fib.wasm OBJECT src/fib.c)
# add_executable(LR src/LR.cpp)
add_executable(prover src/prover.cpp)
add_executable(verifier src/verifier.cpp)
link_binaries(prover verifier)

# add_executable(eval src/eval.cpp)
# link_binaries(eval)
