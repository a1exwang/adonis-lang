cmake_minimum_required(VERSION 3.4.3)
project(al CXX)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
# set(CMAKE_LINKER ld.gold)

FIND_PACKAGE(BISON REQUIRED)
bison_target(parser parser.y ${CMAKE_CURRENT_SOURCE_DIR}/parser.tab.cpp)

# LLVM
set(LLVM_DIR "./llvm-5.0.0.src/build/lib/cmake/llvm")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${PROJECT_SOURCE_DIR}/nvm_malloc/src)
link_directories(${PROJECT_SOURCE_DIR}/nvm_malloc)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_compile_options(-std=c++11)

add_library(alrt SHARED rt/lib.cpp)
target_link_libraries(alrt nvmmalloc)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
llvm_map_components_to_libnames(llvm_compiler_libs support core irreader)
llvm_map_components_to_libnames(llvm_interpreter_libs executionengine x86codegen mcjit)

add_executable(alc alc.cpp al.h al.cpp ${BISON_parser_OUTPUTS} lex.cpp lex.h ast.h ast.cpp compile_time.cpp)
target_link_libraries(alc ${llvm_compiler_libs} re2 nvmmalloc)

add_executable(ali ali.cpp al.h al.cpp rt/lib.cpp ${BISON_parser_OUTPUTS} lex.cpp lex.h ast.h ast.cpp compile_time.cpp)
target_link_libraries(ali alrt ${llvm_compiler_libs} ${llvm_interpreter_libs} re2 nvmmalloc)

add_custom_target(
        alnative
        COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/test.ll
        COMMAND cd ${CMAKE_CURRENT_BINARY_DIR} && ${CMAKE_CURRENT_BINARY_DIR}/alc
        COMMAND test -f ${CMAKE_CURRENT_BINARY_DIR}/test.ll
        COMMAND clang++ -o test -L ${CMAKE_SOURCE_DIR}/nvm_malloc -lnvmmalloc libalrt.so ${CMAKE_CURRENT_BINARY_DIR}/test.ll
)
add_dependencies(alnative alrt alc)

