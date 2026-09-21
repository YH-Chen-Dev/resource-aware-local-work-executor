cmake_minimum_required(VERSION 3.24)

project(resource_aware_local_work_executor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)

FetchContent_MakeAvailable(json)

add_executable(work_executor
    src/main.cpp
)

target_link_libraries(
    work_executor
    PRIVATE
    nlohmann_json::nlohmann_json
)

enable_testing()

add_test(
    NAME missing_manifest_path
    COMMAND work_executor
)

set_tests_properties(
    missing_manifest_path
    PROPERTIES
    WILL_FAIL TRUE
)

add_test(
    NAME single_manifest_argument
    COMMAND work_executor dummy.json
)

add_test(
    NAME too_many_arguments
    COMMAND work_executor a.json b.json
)

set_tests_properties(
    too_many_arguments
    PROPERTIES
    WILL_FAIL TRUE
)
