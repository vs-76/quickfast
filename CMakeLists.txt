cmake_minimum_required(VERSION 3.16)
project(QuickFAST VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(QUICKFAST_BUILD_TESTS "Build QuickFAST unit tests" ON)
option(QUICKFAST_BUILD_EXAMPLES "Build QuickFAST example applications" ON)

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
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(QuickFAST PRIVATE -Wall -Wno-unused-parameter)
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
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)

  file(GLOB QUICKFAST_TEST_SOURCES CONFIGURE_DEPENDS src/Tests/*.cpp)
  list(FILTER QUICKFAST_TEST_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

  add_executable(QuickFASTTest ${QUICKFAST_TEST_SOURCES})
  target_link_libraries(QuickFASTTest
    PRIVATE
      QuickFAST
      GTest::gtest_main
      GTest::gmock
  )
  set_target_properties(QuickFASTTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

  gtest_discover_tests(QuickFASTTest
    PROPERTIES ENVIRONMENT "QUICKFAST_ROOT=${CMAKE_SOURCE_DIR}"
    DISCOVERY_MODE PRE_TEST
  )
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

message(STATUS "QuickFAST ${PROJECT_VERSION}: C++${CMAKE_CXX_STANDARD}, XercesC ${XercesC_VERSION} (no Boost)")
