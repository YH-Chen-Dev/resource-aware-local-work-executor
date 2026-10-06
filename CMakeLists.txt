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

add_library(scheduler_core
    src/scheduler.cpp
)

target_include_directories(
    scheduler_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(work_executor
    src/main.cpp
)

target_link_libraries(
    work_executor
    PRIVATE
    scheduler_core
    nlohmann_json::nlohmann_json
)

add_executable(scheduler_concurrency_test
    tests/scheduler_concurrency_test.cpp
)

target_link_libraries(
    scheduler_concurrency_test
    PRIVATE
    scheduler_core
)

add_executable(scheduler_invalid_concurrency_test
    tests/scheduler_invalid_concurrency_test.cpp
)

target_link_libraries(
    scheduler_invalid_concurrency_test
    PRIVATE
    scheduler_core
)

add_executable(scheduler_cleanup_test
    tests/scheduler_cleanup_test.cpp
)

target_link_libraries(
    scheduler_cleanup_test
    PRIVATE
    scheduler_core
)

enable_testing()

# scheduler_concurrency
add_test(
    NAME scheduler_concurrency
    COMMAND scheduler_concurrency_test
)

set_tests_properties(
    scheduler_concurrency
    PROPERTIES
    TIMEOUT 10
)

# scheduler_invalid_concurrency
add_test(
    NAME scheduler_invalid_concurrency
    COMMAND scheduler_invalid_concurrency_test
)

set_tests_properties(
    scheduler_invalid_concurrency
    PROPERTIES
    TIMEOUT 5
)

# scheduler_cleanup_test
add_test(
    NAME scheduler_cleanup_test
    COMMAND scheduler_cleanup_test
)

set_tests_properties(
    scheduler_cleanup_test
    PROPERTIES
    TIMEOUT 10
)

# executor_concurrency_integration
add_test(
    NAME executor_concurrency_integration
    COMMAND
        /bin/sh
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/concurrency_integration_test.sh
        $<TARGET_FILE:work_executor>
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/concurrency_manifest.json
)

set_tests_properties(
    executor_concurrency_integration
    PROPERTIES
    TIMEOUT 10
)

# missing_manifest_arguments
add_test(
    NAME missing_manifest_arguments
    COMMAND work_executor
)

set_tests_properties(
    missing_manifest_arguments
    PROPERTIES
    WILL_FAIL TRUE
)

# nonexistent_manifest_file
add_test(
    NAME nonexistent_manifest_file
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/does_not_exist.json --concurrency 1
)

set_tests_properties(
    nonexistent_manifest_file
    PROPERTIES
    WILL_FAIL TRUE
)

# too_many_arguments
add_test(
    NAME too_many_arguments
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 1 extra
)

set_tests_properties(
    too_many_arguments
    PROPERTIES
    WILL_FAIL TRUE
)

# valid_manifest
add_test(
    NAME valid_manifest
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 1
)

# valid_manifest_no_arguments
add_test(
    NAME valid_manifest_no_arguments
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest_no_arguments.json --concurrency 1
)

# top_level_object
add_test(
    NAME top_level_object
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/top_level_object.json --concurrency 1
)

set_tests_properties(
    top_level_object
    PROPERTIES
    WILL_FAIL TRUE
)

# missing_executable
add_test(
    NAME missing_executable
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/missing_executable.json --concurrency 1
)

set_tests_properties(
    missing_executable
    PROPERTIES
    WILL_FAIL TRUE
)

# executable_wrong_type
add_test(
    NAME executable_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_wrong_type.json --concurrency 1
)

set_tests_properties(
    executable_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# executable_contains_nul
add_test(
    NAME executable_contains_nul
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_contains_nul.json --concurrency 1
)

set_tests_properties(
    executable_contains_nul
    PROPERTIES
    WILL_FAIL TRUE
)

# executable_empty
add_test(
    NAME executable_empty
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_empty.json --concurrency 1
)

set_tests_properties(
    executable_empty
    PROPERTIES
    WILL_FAIL TRUE
)

# arguments_wrong_type
add_test(
    NAME arguments_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arguments_wrong_type.json --concurrency 1
)

set_tests_properties(
    arguments_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# argument_element_wrong_type
add_test(
    NAME argument_element_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_element_wrong_type.json --concurrency 1
)

set_tests_properties(
    argument_element_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# argument_contains_nul
add_test(
    NAME argument_contains_nul
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_contains_nul.json --concurrency 1
)

set_tests_properties(
    argument_contains_nul
    PROPERTIES
    WILL_FAIL TRUE
)

# valid_optional_id
add_test(
    NAME valid_optional_id
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_optional_id.json --concurrency 1
)

# id_wrong_type
add_test(
    NAME id_wrong_type
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/id_wrong_type.json --concurrency 1
)

set_tests_properties(
    id_wrong_type
    PROPERTIES
    WILL_FAIL TRUE
)

# id_empty
add_test(
    NAME id_empty
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/id_empty.json --concurrency 1
)

set_tests_properties(
    id_empty
    PROPERTIES
    WILL_FAIL TRUE
)

# duplicate_id
add_test(
    NAME duplicate_id
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/duplicate_id.json --concurrency 1
)

set_tests_properties(
    duplicate_id
    PROPERTIES
    WILL_FAIL TRUE
)

# unknown_field
add_test(
    NAME unknown_field
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/unknown_field.json --concurrency 1
)

set_tests_properties(
    unknown_field
    PROPERTIES
    WILL_FAIL TRUE
)

# empty_manifest
add_test(
    NAME empty_manifest
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty_manifest.json --concurrency 1
)

set_tests_properties(
    empty_manifest
    PROPERTIES
    WILL_FAIL TRUE
)

# malformed_manifest
add_test(
    NAME malformed_manifest
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/malformed_manifest.json --concurrency 1
)

set_tests_properties(
    malformed_manifest
    PROPERTIES
    WILL_FAIL TRUE
)

# empty_job_list
add_test(
    NAME empty_job_list
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty_job_list.json --concurrency 1
)

# multiple_valid_jobs
add_test(
    NAME multiple_valid_jobs
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/multiple_valid_jobs.json --concurrency 1
)

set_tests_properties(
    multiple_valid_jobs
    PROPERTIES
    PASS_REGULAR_EXPRESSION "two"
)

# later_invalid_job
add_test(
    NAME later_invalid_job
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/later_invalid_job.json --concurrency 1
)

set_tests_properties(
    later_invalid_job
    PROPERTIES
    WILL_FAIL TRUE
    FAIL_REGULAR_EXPRESSION "SHOULD_NOT_RUN"
)

# nonexistent_executable
add_test(
    NAME nonexistent_executable
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/nonexistent_executable.json --concurrency 1
)

set_tests_properties(
    nonexistent_executable
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Launch: exec failed"
)

# job_element_not_object
add_test(
    NAME job_element_not_object
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/job_element_not_object.json --concurrency 1
)

set_tests_properties(
    job_element_not_object
    PROPERTIES
    WILL_FAIL TRUE
)

# wrong_option_name
add_test(
    NAME wrong_option_name
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --workers 4
)

set_tests_properties(
    wrong_option_name
    PROPERTIES
    WILL_FAIL TRUE
)

# zero_concurrency
add_test(
    NAME zero_concurrency
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 0
)

set_tests_properties(
    zero_concurrency
    PROPERTIES
    WILL_FAIL TRUE
)

# invalid_format
add_test(
    NAME invalid_format
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency abc
)

set_tests_properties(
    invalid_format
    PROPERTIES
    WILL_FAIL TRUE
)

# negative
add_test(
    NAME negative
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency -1
)

set_tests_properties(
    negative
    PROPERTIES
    WILL_FAIL TRUE
)

# plus_sign
add_test(
    NAME plus_sign
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency +4
)

set_tests_properties(
    plus_sign
    PROPERTIES
    WILL_FAIL TRUE
)

# trailing_characters
add_test(
    NAME trailing_characters
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 4abc
)

set_tests_properties(
    trailing_characters
    PROPERTIES
    WILL_FAIL TRUE
)

# leading_whitespace
add_test(
    NAME leading_whitespace
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency " 4"
)

set_tests_properties(
    leading_whitespace
    PROPERTIES
    WILL_FAIL TRUE
)

# trailing_whitespace
add_test(
    NAME trailing_whitespace
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency "4 "
)

set_tests_properties(
    trailing_whitespace
    PROPERTIES
    WILL_FAIL TRUE
)

# out_of_range
add_test(
    NAME out_of_range
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 9999999999999999999999999999999999999999999999999
)

set_tests_properties(
    out_of_range
    PROPERTIES
    WILL_FAIL TRUE
)

# leading_zero
add_test(
    NAME leading_zero
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 04
)
