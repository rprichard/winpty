
cmake_minimum_required(VERSION 3.20)

project(WinPTY
  VERSION 0.4.4
  LANGUAGES CXX)

# Output Locations
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Global Macros
add_compile_definitions(UNICODE)
add_compile_definitions(_UNICODE)
add_compile_definitions(NOMINMAX)

execute_process(COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_REVISION)
  set(GIT_REVISION none)
endif()

add_subdirectory(src)
