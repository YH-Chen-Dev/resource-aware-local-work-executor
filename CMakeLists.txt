cmake_minimum_required(VERSION 3.16)

project(resource_aware_local_work_executor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(work_executor
    src/main.cpp
)

enable_testing()

add_test(
    NAME work_executor_test
    COMMAND work_executor
)
