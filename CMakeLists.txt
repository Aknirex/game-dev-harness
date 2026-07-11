cmake_minimum_required(VERSION 3.28)

project(game-dev-harness VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(harness-core-lib STATIC
  src/harness/tool_registry.cpp
  src/ipc/frame_codec.cpp
  src/ipc/json_rpc.cpp
)
target_include_directories(harness-core-lib PUBLIC src)
target_link_libraries(harness-core-lib PUBLIC nlohmann_json::nlohmann_json)
if(MSVC)
  target_compile_options(harness-core-lib PRIVATE /W4 /permissive-)
else()
  target_compile_options(harness-core-lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(harness-core src/core/main.cpp)
target_link_libraries(harness-core PRIVATE harness-core-lib)

include(CTest)
if(BUILD_TESTING)
  find_package(doctest CONFIG REQUIRED)
  add_executable(harness-tests
    tests/test_main.cpp
    tests/frame_codec_test.cpp
    tests/json_rpc_test.cpp
    tests/tool_registry_test.cpp
  )
  target_link_libraries(harness-tests PRIVATE harness-core-lib doctest::doctest)
  add_test(NAME harness-tests COMMAND harness-tests)
endif()
