# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.
#
# libFuzzer harnesses. Enabled only with -DQUICKFAST_BUILD_FUZZERS=ON and Clang.

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
  message(FATAL_ERROR
    "QUICKFAST_BUILD_FUZZERS=ON requires Clang or AppleClang "
    "(CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID})")
endif()

if(QUICKFAST_SANITIZE_THREAD)
  message(FATAL_ERROR
    "QUICKFAST_BUILD_FUZZERS cannot be combined with QUICKFAST_SANITIZE_THREAD")
endif()

set(QUICKFAST_FUZZ_SANITIZE_FLAGS
  -fsanitize=fuzzer,address,undefined
  -fno-omit-frame-pointer
)

function(quickfast_add_fuzzer name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE QuickFAST)
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
  )
  target_compile_options(${name} PRIVATE
    ${QUICKFAST_WARNING_FLAGS}
    ${QUICKFAST_FUZZ_SANITIZE_FLAGS}
  )
  target_link_options(${name} PRIVATE ${QUICKFAST_FUZZ_SANITIZE_FLAGS})
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
endfunction()

quickfast_add_fuzzer(fuzz_decode_message fuzz_decode_message.cpp)
quickfast_add_fuzzer(fuzz_decimal fuzz_decimal.cpp)
quickfast_add_fuzzer(fuzz_xml_template fuzz_xml_template.cpp)
quickfast_add_fuzzer(fuzz_header_analyzer fuzz_header_analyzer.cpp)
quickfast_add_fuzzer(fuzz_pcap_dissect fuzz_pcap_dissect.cpp)

message(STATUS
  "libFuzzer: fuzz_decode_message fuzz_decimal fuzz_xml_template "
  "fuzz_header_analyzer fuzz_pcap_dissect "
  "(seed corpora under tests/fuzz/corpora/)")
