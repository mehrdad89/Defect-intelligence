cmake_minimum_required(VERSION 3.20)
project(defect_intelligence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(defect-intelligence
  src/main.cpp
  src/core.cpp
  src/http_server.cpp
)

target_include_directories(defect-intelligence PRIVATE include)
target_compile_options(defect-intelligence PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_defect_intelligence
  tests/test_core.cpp
  src/core.cpp
  src/http_server.cpp
)

target_include_directories(test_defect_intelligence PRIVATE include)
target_compile_options(test_defect_intelligence PRIVATE -Wall -Wextra -Wpedantic)

