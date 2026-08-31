# Copyright (c) 2026, QuickFAST contributors.
# All rights reserved.
# See the file license.txt for licensing information.

# C++/CLI wrapper for .NET Framework (MSVC /clr). Requires Windows + MSVC.

set(QUICKFAST_DOTNET_SOURCES
  AssemblyInfo.cpp
  DNDecimal.cpp
  DNDecoderConnection.cpp
  DNField.cpp
  DNFieldSet.cpp
  DNMessageDeliverer.cpp
  DNSequence.cpp
  ImplBuilderBase.cpp
  ImplField.cpp
  ImplFieldSet.cpp
  ImplFieldSetBuilder.cpp
  ImplMessageDeliverer.cpp
  ImplSequence.cpp
  ImplSequenceBuilder.cpp
  Stopwatch.cpp
)

add_library(QuickFASTDotNet SHARED ${QUICKFAST_DOTNET_SOURCES})
add_library(QuickFAST::DotNet ALIAS QuickFASTDotNet)

set_target_properties(QuickFASTDotNet PROPERTIES
  OUTPUT_NAME QuickFASTDotNet
  COMMON_LANGUAGE_RUNTIME ""
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8"
)

target_include_directories(QuickFASTDotNet
  PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(QuickFASTDotNet
  PRIVATE
    QuickFAST
)

# /clr requires asynchronous EH; keep warnings milder than native /W4 /WX.
target_compile_options(QuickFASTDotNet PRIVATE /EHa /W3)
target_compile_definitions(QuickFASTDotNet
  PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

if(QUICKFAST_BUILD_DOTNET_EXAMPLES)
  set(_qf_dotnet_out "$<TARGET_FILE_DIR:QuickFASTDotNet>")
  set(_qf_dotnet_dll "$<TARGET_FILE:QuickFASTDotNet>")
  foreach(_ex InterpretFASTDotNet PerformanceTestDotNet)
    set(_ex_dir "${CMAKE_SOURCE_DIR}/src/DotNetExamples/${_ex}")
    set(_ex_csproj "${_ex_dir}/${_ex}.csproj")
    if(EXISTS "${_ex_csproj}")
      add_custom_target(${_ex} ALL
        COMMAND dotnet build "${_ex_csproj}"
          -c $<IF:$<CONFIG:Debug>,Debug,Release>
          -p:QuickFASTDotNetDll=${_qf_dotnet_dll}
          -o "${_qf_dotnet_out}"
        DEPENDS QuickFASTDotNet
        WORKING_DIRECTORY "${_ex_dir}"
        COMMENT "Building ${_ex} (C# / net48)"
        VERBATIM
      )
    endif()
  endforeach()
endif()
