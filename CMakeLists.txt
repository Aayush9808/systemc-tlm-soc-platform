cmake_minimum_required(VERSION 3.20)

project(soc_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SYSTEMC_ROOT "/opt/homebrew/opt/systemc" CACHE PATH "SystemC installation prefix")

find_path(SYSTEMC_INCLUDE_DIR
    NAMES systemc
    PATHS "\${SYSTEMC_ROOT}/include"
    REQUIRED
)

find_library(SYSTEMC_LIBRARY
    NAMES systemc
    PATHS "\${SYSTEMC_ROOT}/lib"
    REQUIRED
)

enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GENERATED_REGISTER_SOURCES
    generated/src/uart0_registers.cpp
    generated/src/gpio_registers.cpp
    generated/src/rv_timer_registers.cpp
    generated/src/spi_device_registers.cpp
)

add_executable(soc_sim
    src/main.cpp
    \${GENERATED_REGISTER_SOURCES}
)

target_include_directories(soc_sim PRIVATE
    "\${SYSTEMC_INCLUDE_DIR}"
    "\${CMAKE_SOURCE_DIR}/generated/include"
)

target_link_libraries(soc_sim PRIVATE "\${SYSTEMC_LIBRARY}")

add_executable(tlm_demo app/tlm_demo.cpp)
target_include_directories(tlm_demo PRIVATE
    "\${SYSTEMC_INCLUDE_DIR}"
    "\${CMAKE_SOURCE_DIR}/generated/include"
)
target_link_libraries(tlm_demo PRIVATE "\${SYSTEMC_LIBRARY}")

add_executable(tlm_policy_test tests/negative/tlm_policy_test.cpp)
target_include_directories(tlm_policy_test PRIVATE
    "\${SYSTEMC_INCLUDE_DIR}"
    "\${CMAKE_SOURCE_DIR}/generated/include"
)
target_link_libraries(tlm_policy_test PRIVATE "\${SYSTEMC_LIBRARY}")

add_executable(soc_bench benchmarks/soc_bench.cpp)
target_include_directories(soc_bench PRIVATE
    "\${SYSTEMC_INCLUDE_DIR}"
    "\${CMAKE_SOURCE_DIR}/generated/include"
)
target_link_libraries(soc_bench PRIVATE "\${SYSTEMC_LIBRARY}")

add_test(NAME tlm_policy_test COMMAND tlm_policy_test)
add_test(NAME generator_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/generator_test.py)
add_test(NAME tlm_demo_dmi COMMAND tlm_demo)
add_test(NAME soc_boot_timed_lt COMMAND soc_sim --scenario boot --timing timed-lt)
add_test(NAME soc_boot_functional_fast COMMAND soc_sim --scenario boot --timing functional-fast)
