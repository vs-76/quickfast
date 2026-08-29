# Coverage report target for QuickFAST (gcovr + gcov).
# Included only when QUICKFAST_ENABLE_COVERAGE=ON and gcovr is available.

set(QUICKFAST_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
set(QUICKFAST_COVERAGE_HTML_DIR "${QUICKFAST_COVERAGE_DIR}/html")
set(QUICKFAST_COVERAGE_TXT "${QUICKFAST_COVERAGE_DIR}/coverage.txt")

# Match gcov to the C++ compiler major (g++-16 notes are not readable by gcov-15).
string(REGEX MATCH "^[0-9]+" _qf_cxx_major "${CMAKE_CXX_COMPILER_VERSION}")
if(_qf_cxx_major)
  find_program(QUICKFAST_GCOV NAMES "gcov-${_qf_cxx_major}" gcov)
else()
  find_program(QUICKFAST_GCOV NAMES gcov)
endif()
if(NOT QUICKFAST_GCOV)
  message(FATAL_ERROR
    "Coverage enabled but gcov not found (tried gcov-${_qf_cxx_major} and gcov)")
endif()

add_custom_target(coverage
  DEPENDS QuickFASTTest
  COMMAND ${CMAKE_COMMAND} -E make_directory "${QUICKFAST_COVERAGE_HTML_DIR}"
  COMMAND ${CMAKE_COMMAND} -E env QUICKFAST_ROOT=${CMAKE_SOURCE_DIR}
          ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMAND ${QUICKFAST_GCOVR}
          --gcov-executable "${QUICKFAST_GCOV}"
          --root "${CMAKE_SOURCE_DIR}"
          --filter "${CMAKE_SOURCE_DIR}/src/"
          --exclude ".*/_deps/.*"
          --exclude ".*/build.*/_deps/.*"
          --delete
          --print-summary
          --txt "${QUICKFAST_COVERAGE_TXT}"
          --html-details "${QUICKFAST_COVERAGE_HTML_DIR}/index.html"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  USES_TERMINAL
  COMMENT "Running tests and generating gcovr coverage report"
)

message(STATUS "Coverage: target 'coverage' -> ${QUICKFAST_GCOVR} (gcov: ${QUICKFAST_GCOV})")
message(STATUS "Coverage: text ${QUICKFAST_COVERAGE_TXT}")
message(STATUS "Coverage: HTML  ${QUICKFAST_COVERAGE_HTML_DIR}/index.html")
