cmake_minimum_required(VERSION 3.24)

project(resource_aware_local_work_executor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
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
    TIMEOUT 15
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
    TIMEOUT 30
)

# missing_manifest_arguments
set(missing_manifest_arguments_PARAMS
    "$<TARGET_FILE:work_executor>"
    "__END__"
)

add_test(
    NAME missing_manifest_arguments
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${missing_manifest_arguments_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: missing required command-line arguments.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    missing_manifest_arguments
    PROPERTIES
    TIMEOUT 10
)

# nonexistent_manifest_file
set(nonexistent_manifest_file_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/does_not_exist.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME nonexistent_manifest_file
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${nonexistent_manifest_file_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: failed to open manifest file: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/does_not_exist.json\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    nonexistent_manifest_file
    PROPERTIES
    TIMEOUT 10
)

# too_many_arguments
set(too_many_arguments_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "1"
    "extra"
    "__END__"
)

add_test(
    NAME too_many_arguments
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${too_many_arguments_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: unexpected command-line arguments.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    too_many_arguments
    PROPERTIES
    TIMEOUT 10
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
set(top_level_object_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/top_level_object.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME top_level_object
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${top_level_object_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/top_level_object.json\nThe top-level must be an array.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    top_level_object
    PROPERTIES
    TIMEOUT 10
)

# missing_executable
set(missing_executable_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/missing_executable.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME missing_executable
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${missing_executable_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/missing_executable.json\nJob 0: Missing required field: executable\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    missing_executable
    PROPERTIES
    TIMEOUT 10
)

# executable_wrong_type
set(executable_wrong_type_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_wrong_type.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME executable_wrong_type
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${executable_wrong_type_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_wrong_type.json\nJob 0: Field 'executable' must be a string.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    executable_wrong_type
    PROPERTIES
    TIMEOUT 10
)

# executable_contains_nul
set(executable_contains_nul_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_contains_nul.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME executable_contains_nul
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${executable_contains_nul_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_contains_nul.json\nJob 0: Field 'executable' must not contain NUL byte.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    executable_contains_nul
    PROPERTIES
    TIMEOUT 10
)

# executable_empty
set(executable_empty_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_empty.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME executable_empty
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${executable_empty_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/executable_empty.json\nJob 0: Field 'executable' cannot be empty.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    executable_empty
    PROPERTIES
    TIMEOUT 10
)

# arguments_wrong_type
set(arguments_wrong_type_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arguments_wrong_type.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME arguments_wrong_type
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${arguments_wrong_type_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arguments_wrong_type.json\nJob 0: Field 'arguments' must be an array.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    arguments_wrong_type
    PROPERTIES
    TIMEOUT 10
)

# argument_element_wrong_type
set(argument_element_wrong_type_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_element_wrong_type.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME argument_element_wrong_type
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${argument_element_wrong_type_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_element_wrong_type.json\nJob 0: Each element of field 'arguments' must be a string.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    argument_element_wrong_type
    PROPERTIES
    TIMEOUT 10
)

# argument_contains_nul
set(argument_contains_nul_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_contains_nul.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME argument_contains_nul
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${argument_contains_nul_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/argument_contains_nul.json\nJob 0: Each element of field 'arguments' must not contain NUL byte.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    argument_contains_nul
    PROPERTIES
    TIMEOUT 10
)

# valid_optional_id
add_test(
    NAME valid_optional_id
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_optional_id.json --concurrency 1
)

# id_wrong_type
set(id_wrong_type_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/id_wrong_type.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME id_wrong_type
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${id_wrong_type_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/id_wrong_type.json\nJob 0: Field 'id' must be a string.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    id_wrong_type
    PROPERTIES
    TIMEOUT 10
)

# id_empty
set(id_empty_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/id_empty.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME id_empty
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${id_empty_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/id_empty.json\nJob 0: Field 'id' cannot be empty.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    id_empty
    PROPERTIES
    TIMEOUT 10
)

# duplicate_id
set(duplicate_id_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/duplicate_id.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME duplicate_id
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${duplicate_id_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/duplicate_id.json\nJob 2: duplicate id: compile\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    duplicate_id
    PROPERTIES
    TIMEOUT 10
)

# unknown_field
set(unknown_field_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/unknown_field.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME unknown_field
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${unknown_field_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/unknown_field.json\nJob 0: Unknown field: unknown field\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    unknown_field
    PROPERTIES
    TIMEOUT 10
)

# empty_manifest
set(empty_manifest_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty_manifest.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME empty_manifest
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${empty_manifest_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: failed to parse manifest file: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty_manifest.json\n[json.exception.parse_error.101] parse error at line 1, column 1: attempting to parse an empty input; check that your input string or stream contains the expected JSON\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    empty_manifest
    PROPERTIES
    TIMEOUT 10
)

# malformed_manifest
set(malformed_manifest_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/malformed_manifest.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME malformed_manifest
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${malformed_manifest_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: failed to parse manifest file: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/malformed_manifest.json\n[json.exception.parse_error.101] parse error at line 3, column 1: syntax error while parsing value - unexpected end of input; expected '[', '{', or a literal\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    malformed_manifest
    PROPERTIES
    TIMEOUT 10
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
set(later_invalid_job_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/later_invalid_job.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME later_invalid_job
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${later_invalid_job_PARAMS}"
        "-DFORBIDDEN_MESSAGE=SHOULD_NOT_RUN"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/later_invalid_job.json\nJob 1: Field 'executable' must be a string.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    later_invalid_job
    PROPERTIES
    TIMEOUT 10
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
set(job_element_not_object_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/job_element_not_object.json"
    "--concurrency"
    "1"
    "__END__"
)

add_test(
    NAME job_element_not_object
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${job_element_not_object_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: invalid manifest schema: ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/job_element_not_object.json\nJob 0: Job must be a JSON object.\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    job_element_not_object
    PROPERTIES
    TIMEOUT 10
)

# wrong_option_name
set(wrong_option_name_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--workers"
    "4"
    "__END__"
)

add_test(
    NAME wrong_option_name
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${wrong_option_name_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: expected \"--concurrency\" argument.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    wrong_option_name
    PROPERTIES
    TIMEOUT 10
)

# zero_concurrency
set(zero_concurrency_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "0"
    "__END__"
)

add_test(
    NAME zero_concurrency
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${zero_concurrency_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency must be greater than zero.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    zero_concurrency
    PROPERTIES
    TIMEOUT 10
)

# invalid_format
set(invalid_format_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "abc"
    "__END__"
)

add_test(
    NAME invalid_format
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${invalid_format_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is invalid.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    invalid_format
    PROPERTIES
    TIMEOUT 10
)

# negative
set(negative_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "-1"
    "__END__"
)

add_test(
    NAME negative
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${negative_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is invalid.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    negative
    PROPERTIES
    TIMEOUT 10
)

# plus_sign
set(plus_sign_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "+4"
    "__END__"
)

add_test(
    NAME plus_sign
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${plus_sign_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is invalid.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    plus_sign
    PROPERTIES
    TIMEOUT 10
)

# trailing_characters
set(trailing_characters_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "4abc"
    "__END__"
)

add_test(
    NAME trailing_characters
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${trailing_characters_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is invalid.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    trailing_characters
    PROPERTIES
    TIMEOUT 10
)

# leading_whitespace
set(leading_whitespace_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    " 4"
    "__END__"
)

add_test(
    NAME leading_whitespace
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${leading_whitespace_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is invalid.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    leading_whitespace
    PROPERTIES
    TIMEOUT 10
)

# trailing_whitespace
set(trailing_whitespace_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "4 "
    "__END__"
)

add_test(
    NAME trailing_whitespace
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${trailing_whitespace_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is invalid.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    trailing_whitespace
    PROPERTIES
    TIMEOUT 10
)

# out_of_range
set(out_of_range_PARAMS
    "$<TARGET_FILE:work_executor>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json"
    "--concurrency"
    "9999999999999999999999999999999999999999999999999"
    "__END__"
)

add_test(
    NAME out_of_range
    COMMAND ${CMAKE_COMMAND}
        "-DEXEC_ARGS=${out_of_range_PARAMS}"
        "-DEXPECTED_ERROR_MESSAGE=Error: Concurrency value is out of range.\nUsage: work_executor <manifest-path> --concurrency <N>\n"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_failure.cmake"
)

set_tests_properties(
    out_of_range
    PROPERTIES
    TIMEOUT 10
)

# leading_zero
add_test(
    NAME leading_zero
    COMMAND work_executor ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/valid_manifest.json --concurrency 04
)
