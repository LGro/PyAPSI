cmake_minimum_required(VERSION 3.13.4)

project(_pyapsi)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -fPIC")

include(FetchContent)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6
)
FetchContent_MakeAvailable(pybind11)

file(READ external/apsi/CMakeLists.txt APSI_CMAKE_CONTENT)
string(REPLACE "find_package(SEAL 4.1 QUIET REQUIRED)" "find_package(SEAL QUIET REQUIRED)" APSI_CMAKE_CONTENT "${APSI_CMAKE_CONTENT}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/apsi_patched/CMakeLists.txt" "${APSI_CMAKE_CONTENT}")

file(GLOB APSI_SUBDIRS LIST_DIRECTORIES true external/apsi/*)
foreach(SUBDIR ${APSI_SUBDIRS})
    if(IS_DIRECTORY ${SUBDIR})
        get_filename_component(DIRNAME ${SUBDIR} NAME)
        file(COPY ${SUBDIR}/ DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/apsi_patched/${DIRNAME}/")
    endif()
endforeach()

add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/apsi_patched/")

pybind11_add_module(_pyapsi src/main.cpp)

target_link_libraries(_pyapsi PRIVATE pybind11::module apsi)

target_compile_definitions(_pyapsi PRIVATE)

if(APPLE)
    set_target_properties(_pyapsi PROPERTIES
        CXX_VISIBILITY_PRESET "hidden"
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()
