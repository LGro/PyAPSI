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

add_subdirectory(external/apsi/)

pybind11_add_module(_pyapsi src/main.cpp)

target_link_libraries(_pyapsi PRIVATE pybind11::module apsi)

target_compile_definitions(_pyapsi PRIVATE)
