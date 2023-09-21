cmake_minimum_required(VERSION 3.13.4)

project(_pyapsi)
find_package(Threads REQUIRED)
find_package(SEAL 4.1 REQUIRED)
find_package(APSI 0.11.0 REQUIRED)
#find_package(jsoncpp REQUIRED)

# include(FetchContent)

find_package(pybind11 REQUIRED)
#FetchContent_Declare(
#    pybind11
#    GIT_REPOSITORY https://github.com/pybind/pybind11.git
#    GIT_TAG v2.9.2
#)
# FetchContent_MakeAvailable(pybind11)

# add_subdirectory(external/apsi/)
set(MAIN_SOURCES src/sender.cpp src/common_utils.cpp src/csv_reader.cpp src/main.cpp)
set(MAIN_HEADERS src/sender.h src/common_utils.h src/csv_reader.h src/base_clp.h )

pybind11_add_module(_pyapsi ${MAIN_SOURCES} ${MAIN_HEADERS})

target_link_libraries(_pyapsi PRIVATE pybind11::module APSI::apsi SEAL::seal)

target_compile_definitions(_pyapsi PRIVATE)
