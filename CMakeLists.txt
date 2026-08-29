cmake_minimum_required(VERSION 3.16)
project(QuickFAST VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(QUICKFAST_BUILD_TESTS "Build QuickFAST unit tests" ON)
option(QUICKFAST_BUILD_EXAMPLES "Build QuickFAST example applications" ON)
option(QUICKFAST_USE_LIBCXX
  "Use LLVM libc++ instead of the default C++ standard library (Clang only)"
  OFF)
option(QUICKFAST_ENABLE_COVERAGE
  "Instrument QuickFAST and tests for gcov coverage (GCC; use with gcovr)"
  OFF)
option(QUICKFAST_SANITIZE_ADDRESS "Build with AddressSanitizer (-fsanitize=address)" OFF)
option(QUICKFAST_SANITIZE_UNDEFINED "Build with UndefinedBehaviorSanitizer (-fsanitize=undefined)" OFF)
option(QUICKFAST_SANITIZE_THREAD "Build with ThreadSanitizer (-fsanitize=thread)" OFF)
option(QUICKFAST_USE_SPDLOG
  "Build SpdlogLogger + managed_file_sink_mt (spdlog + zlib)"
  OFF)
# The test-only sink hook is compiled into the shared library, so a shipping build
# (QUICKFAST_BUILD_TESTS=OFF) must not carry it.
option(QUICKFAST_ENABLE_TEST_HOOKS
  "Expose managed_file_sink_mt test-only hooks (default: follow QUICKFAST_BUILD_TESTS)"
  ${QUICKFAST_BUILD_TESTS})

if(QUICKFAST_ENABLE_COVERAGE)
  if(NOT QUICKFAST_BUILD_TESTS)
    message(FATAL_ERROR
      "QUICKFAST_ENABLE_COVERAGE=ON requires QUICKFAST_BUILD_TESTS=ON")
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS
      "QUICKFAST_ENABLE_COVERAGE=ON with CMAKE_BUILD_TYPE=Release; "
      "prefer Debug for accurate coverage maps")
  endif()
endif()

if(QUICKFAST_SANITIZE_THREAD AND (QUICKFAST_SANITIZE_ADDRESS OR QUICKFAST_SANITIZE_UNDEFINED))
  message(FATAL_ERROR
    "QUICKFAST_SANITIZE_THREAD cannot be combined with AddressSanitizer or "
    "UndefinedBehaviorSanitizer")
endif()

set(QUICKFAST_SANITIZE_FLAGS "")
if(QUICKFAST_SANITIZE_ADDRESS)
  list(APPEND QUICKFAST_SANITIZE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
if(QUICKFAST_SANITIZE_UNDEFINED)
  list(APPEND QUICKFAST_SANITIZE_FLAGS -fsanitize=undefined -fno-omit-frame-pointer)
endif()
if(QUICKFAST_SANITIZE_THREAD)
  list(APPEND QUICKFAST_SANITIZE_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
  # libstdc++ (esp. GCC 16) diagnoses atomic_thread_fence under TSan; do not
  # promote that into a hard -Werror failure for sanitizer builds.
  list(APPEND QUICKFAST_SANITIZE_FLAGS -Wno-error=tsan)
endif()
if(QUICKFAST_SANITIZE_FLAGS)
  add_compile_options(${QUICKFAST_SANITIZE_FLAGS})
  add_link_options(${QUICKFAST_SANITIZE_FLAGS})
endif()
if(QUICKFAST_USE_LIBCXX)
  if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
          OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
    message(FATAL_ERROR
      "QUICKFAST_USE_LIBCXX=ON requires Clang or AppleClang "
      "(CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID})")
  endif()
  # Propagate to all CXX targets created after this point (including FetchContent
  # deps such as GoogleTest). Prefer CMAKE_CXX_FLAGS over CMAKE_*_LINKER_FLAGS so
  # C try-compiles (e.g. GoogleTest) are not passed -stdlib=.
  if(NOT CMAKE_CXX_FLAGS MATCHES "(^|[ \t])-stdlib=libc\\+\\+([ \t]|$)")
    string(APPEND CMAKE_CXX_FLAGS " -stdlib=libc++")
  endif()
endif()

include(FetchContent)
find_package(Threads REQUIRED)
find_package(XercesC REQUIRED)

# Standalone Asio (header-only)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-30-2
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)
add_library(asio INTERFACE)
add_library(asio::asio ALIAS asio)
target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

if(QUICKFAST_USE_SPDLOG)
  find_package(spdlog 1.12 QUIET CONFIG)
  if(NOT spdlog_FOUND)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      spdlog
      GIT_REPOSITORY https://github.com/gabime/spdlog.git
      GIT_TAG        v1.15.1
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
    if(TARGET spdlog)
      target_compile_options(spdlog PRIVATE -w)
    endif()
  endif()
  find_package(ZLIB REQUIRED)
endif()

# Strict warnings for first-party code (deps use SYSTEM includes / silenced below).
# Exact set required for dual-compiler CI: -Werror -Wall -pedantic
set(QUICKFAST_WARNING_FLAGS -Wall -Werror -pedantic)


set(QUICKFAST_LIB_SOURCES
  src/Application/CommandArgParser.cpp
  src/Application/DecoderConnection.cpp
  src/Application/DNDecoderConnectionImpl.cpp
  src/Codecs/BasePacketAssembler.cpp
  src/Codecs/Context.cpp
  src/Codecs/DataSource.cpp
  src/Codecs/DataSourceBuffer.cpp
  src/Codecs/DataSourceBufferedStream.cpp
  src/Codecs/DataSourceStream.cpp
  src/Codecs/DataSourceString.cpp
  src/Codecs/Decoder.cpp
  src/Codecs/DictionaryIndexer.cpp
  src/Codecs/Encoder.cpp
  src/Codecs/FastEncodedHeaderAnalyzer.cpp
  src/Codecs/FieldInstruction.cpp
  src/Codecs/FieldInstructionAscii.cpp
  src/Codecs/FieldInstructionBlob.cpp
  src/Codecs/FieldInstructionByteVector.cpp
  src/Codecs/FieldInstructionDecimal.cpp
  src/Codecs/FieldInstructionGroup.cpp
  src/Codecs/FieldInstructionSequence.cpp
  src/Codecs/FieldInstructionTemplateRef.cpp
  src/Codecs/FieldInstructionUtf8.cpp
  src/Codecs/FieldOp.cpp
  src/Codecs/FieldOpConstant.cpp
  src/Codecs/FieldOpCopy.cpp
  src/Codecs/FieldOpDefault.cpp
  src/Codecs/FieldOpDelta.cpp
  src/Codecs/FieldOpIncrement.cpp
  src/Codecs/FieldOpTail.cpp
  src/Codecs/FixedSizeHeaderAnalyzer.cpp
  src/Codecs/GenericMessageBuilder.cpp
  src/Codecs/MessagePerPacketAssembler.cpp
  src/Codecs/MulticastDecoder.cpp
  src/Codecs/PacketSequencingAssembler.cpp
  src/Codecs/PresenceMap.cpp
  src/Codecs/SchemaElement.cpp
  src/Codecs/SegmentBody.cpp
  src/Codecs/SingleMessageConsumer.cpp
  src/Codecs/StreamingAssembler.cpp
  src/Codecs/Template.cpp
  src/Codecs/TemplateRegistry.cpp
  src/Codecs/XMLTemplateParser.cpp
  src/Common/BitMap.cpp
  src/Common/Decimal.cpp
  src/Common/Profiler.cpp
  src/Common/QuickFASTPch.cpp
  src/Common/Types.cpp
  src/Common/WorkingBuffer.cpp
  src/Communication/AsioService.cpp
  src/Communication/AsynchFileSender.cpp
  src/Communication/AsynchSender.cpp
  src/Communication/FileSender.cpp
  src/Communication/MulticastReceiverHandle.cpp
  src/Communication/PCapReader.cpp
  src/Messages/Field.cpp
  src/Messages/FieldAscii.cpp
  src/Messages/FieldByteVector.cpp
  src/Messages/FieldDecimal.cpp
  src/Messages/FieldGroup.cpp
  src/Messages/FieldIdentity.cpp
  src/Messages/FieldInt16.cpp
  src/Messages/FieldInt32.cpp
  src/Messages/FieldInt64.cpp
  src/Messages/FieldInt8.cpp
  src/Messages/FieldSequence.cpp
  src/Messages/FieldSet.cpp
  src/Messages/FieldString.cpp
  src/Messages/FieldUInt16.cpp
  src/Messages/FieldUInt32.cpp
  src/Messages/FieldUInt64.cpp
  src/Messages/FieldUInt8.cpp
  src/Messages/FieldUtf8.cpp
  src/Messages/Message.cpp
  src/Messages/MessageAccessor.cpp
  src/Messages/MessageBuilder.cpp
  src/Messages/MessageFormatter.cpp
)

if(WIN32)
  list(APPEND QUICKFAST_LIB_SOURCES src/Common/dllmain.cpp)
endif()

if(QUICKFAST_USE_SPDLOG)
  list(APPEND QUICKFAST_LIB_SOURCES
    src/Common/SpdlogLogger.cpp
    src/Common/ManagedFileSink.cpp)
endif()

add_library(QuickFAST SHARED ${QUICKFAST_LIB_SOURCES})
add_library(QuickFAST::QuickFAST ALIAS QuickFAST)

target_compile_definitions(QuickFAST
  PRIVATE
    QUICKFAST_BUILD_DLL
)

target_include_directories(QuickFAST
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(QuickFAST
  PUBLIC
    asio
    Threads::Threads
    XercesC::XercesC
)

if(QUICKFAST_USE_SPDLOG)
  target_compile_definitions(QuickFAST PUBLIC QUICKFAST_HAS_SPDLOG=1)
  target_link_libraries(QuickFAST PUBLIC spdlog::spdlog ZLIB::ZLIB)
  if(QUICKFAST_ENABLE_TEST_HOOKS)
    target_compile_definitions(QuickFAST PUBLIC QUICKFAST_ENABLE_TEST_HOOKS=1)
  endif()
endif()

target_compile_options(QuickFAST PRIVATE ${QUICKFAST_WARNING_FLAGS})
if(QUICKFAST_ENABLE_COVERAGE)
  target_compile_options(QuickFAST PRIVATE --coverage)
  target_link_options(QuickFAST PRIVATE --coverage)
endif()

set_target_properties(QuickFAST PROPERTIES
  OUTPUT_NAME QuickFAST
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

if(QUICKFAST_BUILD_TESTS)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(googletest)
  foreach(_gtest_target gtest gtest_main gmock gmock_main)
    if(TARGET ${_gtest_target})
      target_compile_options(${_gtest_target} PRIVATE -w)
    endif()
  endforeach()

  enable_testing()
  include(GoogleTest)

  file(GLOB QUICKFAST_TEST_SOURCES CONFIGURE_DEPENDS src/Tests/*.cpp)
  list(FILTER QUICKFAST_TEST_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
  if(NOT QUICKFAST_USE_SPDLOG)
    list(FILTER QUICKFAST_TEST_SOURCES EXCLUDE REGEX ".*/testSpdlogLogger\\.cpp$")
    list(FILTER QUICKFAST_TEST_SOURCES EXCLUDE REGEX ".*/testManagedFileSink\\.cpp$")
  endif()

  add_executable(QuickFASTTest ${QUICKFAST_TEST_SOURCES})
  target_link_libraries(QuickFASTTest
    PRIVATE
      QuickFAST
      GTest::gtest_main
      GTest::gmock
  )
  target_compile_options(QuickFASTTest PRIVATE ${QUICKFAST_WARNING_FLAGS})
  if(QUICKFAST_ENABLE_COVERAGE)
    target_compile_options(QuickFASTTest PRIVATE --coverage)
    target_link_options(QuickFASTTest PRIVATE --coverage)
  endif()
  set_target_properties(QuickFASTTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

  gtest_discover_tests(QuickFASTTest
    PROPERTIES ENVIRONMENT "QUICKFAST_ROOT=${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )

  if(QUICKFAST_ENABLE_COVERAGE)
    find_program(QUICKFAST_GCOVR NAMES gcovr)
    if(QUICKFAST_GCOVR)
      include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Coverage.cmake")
    else()
      message(STATUS
        "Coverage: gcovr not found; instrumentation enabled but target "
        "'coverage' not created (install gcovr)")
    endif()
  endif()
endif()

if(QUICKFAST_BUILD_EXAMPLES)
  set(QUICKFAST_EXAMPLE_COMMON
    src/Examples/Examples/ConsoleLock.cpp
    src/Examples/Examples/ExamplesPch.cpp
    src/Examples/Examples/MessageInterpreter.cpp
    src/Examples/Examples/MessagePerformance.cpp
    src/Examples/Examples/ValueToFix.cpp
  )

  function(quickfast_add_example name)
    add_executable(${name} ${ARGN} ${QUICKFAST_EXAMPLE_COMMON})
    target_include_directories(${name} PRIVATE src/Examples)
    target_link_libraries(${name} PRIVATE QuickFAST)
    target_compile_options(${name} PRIVATE ${QUICKFAST_WARNING_FLAGS})
    set_target_properties(${name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
  endfunction()

  quickfast_add_example(InterpretApplication
    src/Examples/InterpretApplication/InterpretApplication.cpp
    src/Examples/InterpretApplication/main.cpp)

  quickfast_add_example(TutorialApplication
    src/Examples/TutorialApplication/TutorialApplication.cpp
    src/Examples/TutorialApplication/main.cpp)

  quickfast_add_example(PerformanceTest
    src/Examples/PerformanceTest/PerformanceTest.cpp
    src/Examples/PerformanceTest/MessageCounter.cpp
    src/Examples/PerformanceTest/NullMessage.cpp
    src/Examples/PerformanceTest/main.cpp)

  quickfast_add_example(FileToTCP
    src/Examples/FileToTCP/FileToTCP.cpp
    src/Examples/FileToTCP/main.cpp)

  quickfast_add_example(FileToMulticast
    src/Examples/FileToMulticast/FileToMulticast.cpp
    src/Examples/FileToMulticast/main.cpp)

  quickfast_add_example(PCapToMulticast
    src/Examples/PCapToMulticast/PCapToMulticast.cpp
    src/Examples/PCapToMulticast/main.cpp)
endif()

# Optional PVS-Studio static analysis target (created only when the tools exist).
option(QUICKFAST_ENABLE_PVS_STUDIO
  "Create a pvs-studio CMake target when pvs-studio-analyzer is available"
  ON)

if(QUICKFAST_ENABLE_PVS_STUDIO)
  find_program(QUICKFAST_PVS_STUDIO_ANALYZER NAMES pvs-studio-analyzer)
  find_program(QUICKFAST_PVS_STUDIO_CONVERTER NAMES plog-converter)
  if(QUICKFAST_PVS_STUDIO_ANALYZER AND QUICKFAST_PVS_STUDIO_CONVERTER)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PVS-Studio.cmake")
    pvs_studio_add_target(
      TARGET pvs-studio
      ANALYZE QuickFAST
      MODE GA:1,2
      OUTPUT
      LOG "${CMAKE_BINARY_DIR}/pvs-studio.log"
      FORMAT errorfile
      BIN "${QUICKFAST_PVS_STUDIO_ANALYZER}"
      CONVERTER "${QUICKFAST_PVS_STUDIO_CONVERTER}"
      ARGS
        -e "${CMAKE_BINARY_DIR}/_deps"
        -e "${CMAKE_CURRENT_SOURCE_DIR}/src/DotNet"
    )
    message(STATUS "PVS-Studio: target 'pvs-studio' -> ${QUICKFAST_PVS_STUDIO_ANALYZER}")
  else()
    message(STATUS "PVS-Studio: analyzer not found; target 'pvs-studio' not created")
  endif()
endif()

message(STATUS "QuickFAST ${PROJECT_VERSION}: C++${CMAKE_CXX_STANDARD}, XercesC ${XercesC_VERSION} (no Boost)")
message(STATUS "Warning flags: ${QUICKFAST_WARNING_FLAGS}")
if(QUICKFAST_USE_LIBCXX)
  message(STATUS "C++ standard library: libc++ (-stdlib=libc++)")
else()
  message(STATUS "C++ standard library: default (set -DQUICKFAST_USE_LIBCXX=ON for libc++ with Clang)")
endif()
if(QUICKFAST_ENABLE_COVERAGE)
  message(STATUS "Coverage: instrumentation ON (--coverage on QuickFAST + QuickFASTTest)")
endif()
if(QUICKFAST_SANITIZE_FLAGS)
  message(STATUS "Sanitizers: ${QUICKFAST_SANITIZE_FLAGS}")
endif()
if(QUICKFAST_USE_SPDLOG)
  message(STATUS "spdlog: SpdlogLogger adapter enabled (QUICKFAST_HAS_SPDLOG)")
else()
  message(STATUS "spdlog: OFF (set -DQUICKFAST_USE_SPDLOG=ON for Common::SpdlogLogger)")
endif()
