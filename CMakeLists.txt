cmake_minimum_required(VERSION 3.20)

project(soc_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional installation prefix. If omitted, CMake also checks SYSTEMC_HOME,
# common Homebrew prefixes, and its normal platform search paths.
set(SYSTEMC_ROOT "" CACHE PATH "SystemC installation prefix")
set(_SYSTEMC_HINTS)
if(SYSTEMC_ROOT)
    list(APPEND _SYSTEMC_HINTS "${SYSTEMC_ROOT}")
endif()
if(DEFINED ENV{SYSTEMC_HOME} AND NOT "$ENV{SYSTEMC_HOME}" STREQUAL "")
    list(APPEND _SYSTEMC_HINTS "$ENV{SYSTEMC_HOME}")
endif()
list(APPEND _SYSTEMC_HINTS
    /opt/homebrew/opt/systemc
    /usr/local/opt/systemc
)

find_path(SYSTEMC_INCLUDE_DIR
    NAMES systemc
    HINTS ${_SYSTEMC_HINTS}
    PATH_SUFFIXES include
    REQUIRED
)

find_library(SYSTEMC_LIBRARY
    NAMES systemc
    HINTS ${_SYSTEMC_HINTS}
    PATH_SUFFIXES lib lib64
    REQUIRED
)

enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GENERATED_REGISTER_SOURCES
    generated/src/uart0_registers.cpp
    generated/src/gpio_registers.cpp
    generated/src/rv_timer_registers.cpp
    generated/src/spi_device_registers.cpp
    generated/src/irq_registers.cpp
)

add_executable(soc_sim
    src/main.cpp
    ${GENERATED_REGISTER_SOURCES}
)

target_include_directories(soc_sim PRIVATE
    "${SYSTEMC_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/generated/include"
)

target_link_libraries(soc_sim PRIVATE "${SYSTEMC_LIBRARY}")

add_executable(tlm_demo app/tlm_demo.cpp ${GENERATED_REGISTER_SOURCES})

target_include_directories(tlm_demo PRIVATE
    "${SYSTEMC_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/generated/include"
)

target_link_libraries(tlm_demo PRIVATE "${SYSTEMC_LIBRARY}")

add_executable(tlm_policy_test tests/negative/tlm_policy_test.cpp)

target_include_directories(tlm_policy_test PRIVATE
    "${SYSTEMC_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/generated/include"
)

target_link_libraries(tlm_policy_test PRIVATE "${SYSTEMC_LIBRARY}")

add_executable(soc_bench benchmarks/soc_bench.cpp ${GENERATED_REGISTER_SOURCES})

target_include_directories(soc_bench PRIVATE
    "${SYSTEMC_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/generated/include"
)

target_link_libraries(soc_bench PRIVATE "${SYSTEMC_LIBRARY}")

add_test(
    NAME tlm_policy_test
    COMMAND tlm_policy_test
)

add_test(
    NAME generator_validation
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/generator_test.py
)

set_tests_properties(generator_validation PROPERTIES
    ENVIRONMENT
        "SYSTEMC_INCLUDE_DIR=${SYSTEMC_INCLUDE_DIR};SYSTEMC_LIBRARY=${SYSTEMC_LIBRARY}"
)

add_test(
    NAME image_initialization_errors
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/image_error_test.py
)

set_tests_properties(image_initialization_errors PROPERTIES
    ENVIRONMENT
        "SYSTEMC_INCLUDE_DIR=${SYSTEMC_INCLUDE_DIR};SYSTEMC_LIBRARY=${SYSTEMC_LIBRARY}"
)

add_test(
    NAME tlm_demo_dmi
    COMMAND tlm_demo
)

add_test(
    NAME soc_boot_timed_lt
    COMMAND soc_sim --scenario boot --timing timed-lt
)

add_test(
    NAME soc_boot_functional_fast
    COMMAND soc_sim --scenario boot --timing functional-fast
)
