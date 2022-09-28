cmake_minimum_required(VERSION 3.10)

# Set compilers to default to Clang is not set.
if ("$ENV{CC}" STREQUAL "")
  set(ENV{CC} clang)
endif()
if ("$ENV{CXX}" STREQUAL "")
  set(ENV{CXX} clang++)
endif()


project(zuckerli)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-fcolor-diagnostics -Wall)

find_package(Threads REQUIRED)

add_subdirectory(abseil-cpp)

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
  src/flags.cc
  src/common.h
)

target_link_libraries(common absl::flags absl::flags_parse)

add_executable(common_test src/common_test.cc)
target_link_libraries(common_test common gmock gtest_main gtest Threads::Threads)
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
target_link_libraries(bits_test bit_writer bit_reader gmock gtest_main gtest Threads::Threads)
gtest_discover_tests(bits_test)

# A library cannot contain just a headerfile.
#add_library(
#  entropy_coder_common
#  src/integer_coder.h
#)
add_library(entropy_coder_common INTERFACE)
target_include_directories(entropy_coder_common INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCDIR}>/src)

target_link_libraries(entropy_coder_common INTERFACE bit_reader bit_writer)

add_executable(entropy_coder_common_test src/entropy_coder_common_test.cc)
target_link_libraries(entropy_coder_common_test entropy_coder_common gmock gtest_main gtest Threads::Threads)
gtest_discover_tests(entropy_coder_common_test)

add_library(
  huffman
  src/huffman.cc
  src/huffman.h
)
target_link_libraries(huffman entropy_coder_common)

add_executable(huffman_test src/huffman_test.cc)
target_link_libraries(huffman_test huffman gmock gtest_main gtest Threads::Threads)
gtest_discover_tests(huffman_test)

add_library(
  ans
  src/ans.cc
  src/ans.h
)
target_link_libraries(ans entropy_coder_common)

add_executable(ans_test src/ans_test.cc)
target_link_libraries(ans_test ans gmock gtest_main gtest Threads::Threads)
gtest_discover_tests(ans_test)

add_library(
  uncompressed_graph
  src/uncompressed_graph.cc
  src/uncompressed_graph.h
)
target_link_libraries(uncompressed_graph entropy_coder_common)

add_executable(uncompressed_graph_test src/uncompressed_graph_test.cc)
target_link_libraries(uncompressed_graph_test uncompressed_graph gmock gtest_main gtest Threads::Threads)
gtest_discover_tests(uncompressed_graph_test)

target_compile_definitions(uncompressed_graph_test PRIVATE
        -DTESTDATA="${CMAKE_CURRENT_SOURCE_DIR}/testdata")

add_executable(traversal_main_uncompressed src/traversal_main_uncompressed.cc)
target_link_libraries(traversal_main_uncompressed uncompressed_graph Threads::Threads)

add_library(encode src/encode.h src/encode.cc src/context_model.h src/checksum.h)
target_link_libraries(encode ans huffman uncompressed_graph)


# A library cannot contain just headerfiles.
#add_library(decode src/decode.h src/context_model.h src/checksum.h)

add_library(decode INTERFACE)
target_include_directories(decode INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCDIR}>/src)

target_link_libraries(decode INTERFACE ans huffman)


add_library(
  compressed_graph
  src/compressed_graph.cc
  src/compressed_graph.h
)
target_link_libraries(compressed_graph decode)

add_executable(traversal_main_compressed src/traversal_main_compressed.cc)
target_link_libraries(traversal_main_compressed compressed_graph Threads::Threads)


add_executable(roundtrip_test src/roundtrip_test.cc)
target_link_libraries(roundtrip_test encode decode uncompressed_graph gmock gtest_main gtest Threads::Threads)

target_compile_definitions(roundtrip_test PRIVATE
        -DTESTDATA="${CMAKE_CURRENT_SOURCE_DIR}/testdata")

add_executable(encoder src/encode_main.cc)
target_link_libraries(encoder encode)

add_executable(decoder src/decode_main.cc)
target_link_libraries(decoder decode)
