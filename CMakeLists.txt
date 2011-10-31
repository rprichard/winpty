cmake_minimum_required(VERSION 2.8)
project(Scrape)
set(CMAKE_BUILD_TYPE Debug)

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "WINVER=0x0501")

add_executable(
    Test
    Test.cc
    Client.cc
    AgentIo.cc
    DebugClient.cc
)
add_executable(
    Agent
    Agent.cc
    AgentIo.cc
    DebugClient.cc
)
