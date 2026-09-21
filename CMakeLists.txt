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

# missing_manifest_path
add_test(
    NAME missing_manifest_path
    COMMAND work_executor
)

set_tests_properties(
    missing_manifest_path
    PROPERTIES
    WILL_FAIL TRUE
)

# nonexistent_manifest_file
add_test(
    NAME nonexistent_manifest_file
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/does_not_exist.json
)

set_tests_properties(
    nonexistent_manifest_file
    PROPERTIES
    WILL_FAIL TRUE
)

# too_many_arguments
add_test(
    NAME too_many_arguments
    COMMAND work_executor a.json b.json
)

set_tests_properties(
    too_many_arguments
    PROPERTIES
    WILL_FAIL TRUE
)

# valid_manifest
add_test(
    NAME valid_manifest
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json
)

# valid_manifest_no_arguments
add_test(
    NAME valid_manifest_no_arguments
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest_no_arguments.json
)

# top_level_array
add_test(
    NAME top_level_array
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/top_level_array.json
)

set_tests_properties(
    top_level_array
    PROPERTIES
    WILL_FAIL TRUE
)

# missing_executable
add_test(
    NAME missing_executable
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/missing_executable.json
)

set_tests_properties(
    missing_executable
    PROPERTIES
    WILL_FAIL TRUE
)

# executable_wrong_type
add_test(
    NAME executable_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_wrong_type.json
)

set_tests_properties(
    executable_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# executable_empty
add_test(
    NAME executable_empty
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_empty.json
)

set_tests_properties(
    executable_empty
    PROPERTIES
    WILL_FAIL TRUE
)

# arguments_wrong_type
add_test(
    NAME arguments_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arguments_wrong_type.json
)

set_tests_properties(
    arguments_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# argument_element_wrong_type
add_test(
    NAME argument_element_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_element_wrong_type.json
)

set_tests_properties(
    argument_element_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# unknown_field
add_test(
    NAME unknown_field
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/unknown_field.json
)

set_tests_properties(
    unknown_field
    PROPERTIES
    WILL_FAIL TRUE
)

# empty_manifest
add_test(
    NAME empty_manifest
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty_manifest.json
)

set_tests_properties(
    empty_manifest
    PROPERTIES
    WILL_FAIL TRUE
)

# malformed_manifest
add_test(
    NAME malformed_manifest
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/malformed_manifest.json
)

set_tests_properties(
    malformed_manifest
    PROPERTIES
    WILL_FAIL TRUE
)
