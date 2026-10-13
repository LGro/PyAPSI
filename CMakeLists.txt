cmake_minimum_required(VERSION 3.25)

project(_pyapsi)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# APSI assembly files (FourQlib) use AVX2 instructions and non-PIC relocations,
# which are incompatible with Python shared extensions on Linux.
# The generic C fallback is used instead, resulting in ~2-3x slower OPRF operations.
# See docs/README.md for details.
set(APSI_USE_ASM OFF CACHE BOOL "Use ASM implementation" FORCE)

# PyAPSI exposes only APSI's "advanced" API on top of network::StreamChannel, so the
# ZeroMQ based channel and the command line interface are not needed. Dropping them also
# drops the ZeroMQ/cppzmq dependency from the build (see VCPKG_DEPENDENCIES in setup.py).
set(APSI_USE_ZMQ OFF CACHE BOOL "Use ZeroMQ for networking" FORCE)
set(APSI_BUILD_CLI OFF CACHE BOOL "Build example command line interface applications" FORCE)
set(APSI_BUILD_TESTS OFF CACHE BOOL "Build unit and integration tests for APSI" FORCE)

include(FetchContent)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6
)
FetchContent_MakeAvailable(pybind11)

add_subdirectory(external/apsi)

# APSI's vendored FourQ headers are configured into ${CMAKE_BINARY_DIR}/common/apsi/fourq/,
# which equals its own include path only in a standalone APSI build. Here APSI is a
# subdirectory, so ${CMAKE_BINARY_DIR} is PyAPSI's build root and that path has to be
# added for APSI's own sources to find "apsi/fourq/...".
target_include_directories(apsi PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/common>)

pybind11_add_module(_pyapsi src/main.cpp)

target_link_libraries(_pyapsi PRIVATE pybind11::module apsi)

target_compile_definitions(_pyapsi PRIVATE)

if(APPLE)
    set_target_properties(_pyapsi PROPERTIES
        CXX_VISIBILITY_PRESET "hidden"
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()
