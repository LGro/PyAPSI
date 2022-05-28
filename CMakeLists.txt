cmake_minimum_required(VERSION 3.18.4)

project(_pyapsi)

include(FetchContent)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.9.2
)
FetchContent_MakeAvailable(pybind11)

find_package(APSI CONFIG REQUIRED)

pybind11_add_module(_pyapsi src/main.cpp)

set_target_properties(_pyapsi PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(_pyapsi PRIVATE pybind11::module APSI::apsi)

target_compile_definitions(_pyapsi PRIVATE)
