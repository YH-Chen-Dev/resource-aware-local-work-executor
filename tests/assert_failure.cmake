if(NOT DEFINED EXPECTED_ERROR_MESSAGE)
    message(FATAL_ERROR "Error: lack of EXPECTED_ERROR_MESSAGE")
endif()

if(EXPECTED_ERROR_MESSAGE STREQUAL "")
    message(FATAL_ERROR "Error: EXPECTED_ERROR_MESSAGE must not be empty string")
endif()

list(POP_BACK EXEC_ARGS LAST_ARG)

if(NOT LAST_ARG STREQUAL __END__)
    message(FATAL_ERROR "Error: last argument must be \"__END__\"")
endif()

execute_process(
    COMMAND ${EXEC_ARGS}
    RESULT_VARIABLE EXIT_CODE
    OUTPUT_VARIABLE OUTPUT_MESSAGE
    ERROR_VARIABLE ERROR_MESSAGE
    TIMEOUT 5
)

if(NOT EXIT_CODE MATCHES "^[1-9][0-9]*$")
    message(STATUS "Exit code: ${EXIT_CODE}")
    message(FATAL_ERROR "Error: the test expected to be error")
endif()

if(DEFINED FORBIDDEN_MESSAGE)
    string(FIND "${OUTPUT_MESSAGE}" "${FORBIDDEN_MESSAGE}" POSITION)
    if(NOT POSITION EQUAL -1)
        message(FATAL_ERROR "Error: the forbidden message is not expected to output.")
    endif()
endif()

if(NOT "${ERROR_MESSAGE}" STREQUAL "${EXPECTED_ERROR_MESSAGE}")
    message(STATUS "Expected error message: ${EXPECTED_ERROR_MESSAGE}")
    message(STATUS "Actual error message: ${ERROR_MESSAGE}")
    message(FATAL_ERROR "Error: the error message does not match.")
endif()

message(STATUS "Pass.")
