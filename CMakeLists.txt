cmake_minimum_required(VERSION 3.10)

# Set compilers to default to Clang is not set.
if ("$ENV{CC}" STREQUAL "")
  set(ENV{CC} clang)
endif()
if ("$ENV{CXX}" STREQUAL "")
  set(ENV{CXX} clang++)
endif()


project(zuckerli)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-fcolor-diagnostics -Wall)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

enable_testing()
include(CTest)
cmake_policy(SET CMP0057 NEW)  # https://gitlab.kitware.com/cmake/cmake/issues/18198
include(GoogleTest)

add_library(
  common
  src/common.cc
  src/common.h
)

add_executable(common_test src/common_test.cc)
target_link_libraries(common_test common gmock gtest gtest_main) 
gtest_discover_tests(common_test)

add_library(
  bit_reader
  src/bit_reader.cc
  src/bit_reader.h
)
target_link_libraries(bit_reader common)

add_library(
  bit_writer
  src/bit_writer.cc
  src/bit_writer.h
)
target_link_libraries(bit_writer common)

add_executable(bits_test src/bits_test.cc)
target_link_libraries(bits_test bit_writer bit_reader gmock gtest gtest_main) 
gtest_discover_tests(bits_test)

add_library(
  entropy_coder_common
  src/integer_coder.h
)
target_link_libraries(entropy_coder_common bit_reader bit_writer)

add_executable(entropy_coder_common_test src/entropy_coder_common_test.cc)
target_link_libraries(entropy_coder_common_test entropy_coder_common gmock gtest gtest_main) 
gtest_discover_tests(entropy_coder_common_test)

add_library(
  huffman
  src/huffman.cc
  src/huffman.h
)
target_link_libraries(huffman entropy_coder_common)

add_executable(huffman_test src/huffman_test.cc)
target_link_libraries(huffman_test huffman gmock gtest gtest_main) 
gtest_discover_tests(huffman_test)

add_library(
  ans
  src/ans.cc
  src/ans.h
)
target_link_libraries(ans entropy_coder_common)

add_executable(ans_test src/ans_test.cc)
target_link_libraries(ans_test ans gmock gtest gtest_main) 
gtest_discover_tests(ans_test)


