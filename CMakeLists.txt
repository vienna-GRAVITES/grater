cmake_minimum_required(VERSION 3.16)
project(grater)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(WIN32)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_DEBUG "/MD /Zi /DDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /DNDEBUG /Zi")
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Embed the exact directory of libTimeTagger.so directly into the RPATH of
# every target, so the built binaries always resolve it at the precise path
# it was linked against — never via system search paths, ldconfig cache, or
# LD_LIBRARY_PATH.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

find_package(Python 3.12 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "Python_EXECUTABLE: ${Python_EXECUTABLE}")

# --- Require Swabian-TimeTagger to already be installed in this environment ---
execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import Swabian.TimeTagger as TT, os; print(os.path.dirname(TT.__file__))"
    OUTPUT_VARIABLE SWABIAN_PACKAGE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE SWABIAN_IMPORT_RESULT
    ERROR_VARIABLE SWABIAN_IMPORT_ERROR
)

if(NOT SWABIAN_IMPORT_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Could not import Swabian.TimeTagger using ${Python_EXECUTABLE}.\n"
        "Swabian-TimeTagger must be installed in the build environment before configuring.\n"
        "If using uv with build isolation, add it to [build-system] requires "
        "in pyproject.toml, not just [project] dependencies, or build with "
        "--no-build-isolation.\n"
        "Python error output:\n${SWABIAN_IMPORT_ERROR}"
    )
endif()

message(STATUS "Swabian package found at: ${SWABIAN_PACKAGE_DIR}")

execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import os; print('\\n'.join(os.listdir('${SWABIAN_PACKAGE_DIR}')))"
    OUTPUT_VARIABLE SWABIAN_PACKAGE_CONTENTS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Assert the exact library path directly — no searching, no fallback.
if(WIN32)
    set(TIMETAGGER_LIBRARY "${SWABIAN_PACKAGE_DIR}/TimeTagger.dll")
else()
    set(TIMETAGGER_LIBRARY "${SWABIAN_PACKAGE_DIR}/libTimeTagger.so")
endif()

if(NOT EXISTS "${TIMETAGGER_LIBRARY}")
    message(FATAL_ERROR
        "Expected the TimeTagger shared library at:\n"
        "  ${TIMETAGGER_LIBRARY}\n"
        "but it wasn't found. Contents of ${SWABIAN_PACKAGE_DIR}:\n"
        "${SWABIAN_PACKAGE_CONTENTS}"
    )
endif()

message(STATUS "TimeTagger library: ${TIMETAGGER_LIBRARY}")

# Headers still come from the system SDK install.
if(WIN32)
    find_path(TIMETAGGER_INCLUDE_DIR
        NAMES TimeTagger.h
        PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/driver/include"
            "C:/Program Files/Swabian Instruments/Time Tagger/driver/include"
            "C:/Program Files (x86)/Swabian Instruments/Time Tagger/driver/include"
            ENV TIMETAGGER_ROOT
        PATH_SUFFIXES driver/include include
    )
else()
    find_path(TIMETAGGER_INCLUDE_DIR
        NAMES TimeTagger.h
        PATHS
            /usr/include/timetagger
            /usr/local/include/timetagger
            /opt/timetagger/include
            ENV TIMETAGGER_ROOT
        PATH_SUFFIXES include
    )
endif()

if(NOT TIMETAGGER_INCLUDE_DIR)
    message(FATAL_ERROR "TimeTagger include directory not found. Please set TIMETAGGER_ROOT environment variable or install the TimeTagger SDK.")
endif()

message(STATUS "TimeTagger include directory: ${TIMETAGGER_INCLUDE_DIR}")

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    nanobind
    GIT_REPOSITORY https://github.com/wjakob/nanobind.git
    GIT_TAG        v2.2.0
)
FetchContent_MakeAvailable(nanobind)

FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20260817.0     # verify this tag exists on Abseil's releases page
)
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_BUILD_TESTING OFF)
FetchContent_MakeAvailable(abseil-cpp)

include_directories(src/grater/include)
include_directories(${TIMETAGGER_INCLUDE_DIR})

file(GLOB_RECURSE SOURCES "src/grater/src/*.cpp")

# --- process executable removed: no source file currently defines main().
# Re-add if/when a CLI entry point exists again:
#
# add_executable(process ${SOURCES})
# target_link_libraries(process PRIVATE ${TIMETAGGER_LIBRARY} nlohmann_json::nlohmann_json)
# target_link_libraries(process PRIVATE absl::flat_hash_map)
# set_target_properties(process PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
# )
# if(WIN32 AND EXISTS "${TIMETAGGER_LIBRARY}")
#     add_custom_command(TARGET process POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         ${TIMETAGGER_LIBRARY}
#         $<TARGET_FILE_DIR:process>
#         COMMENT "Copying TimeTagger DLL to output directory"
#     )
# endif()

# Create Python module using nanobind
set(PYTHON_BINDING_SOURCES
    ${SOURCES}
    "src/grater/python/bindings.cpp"
)

nanobind_add_module(grater ${PYTHON_BINDING_SOURCES})
target_link_libraries(grater PRIVATE ${TIMETAGGER_LIBRARY} nlohmann_json::nlohmann_json absl::flat_hash_map)

set_target_properties(grater PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
    BUILD_RPATH "${SWABIAN_PACKAGE_DIR}"
    INSTALL_RPATH "${SWABIAN_PACKAGE_DIR}"
)
install(
    TARGETS grater
    LIBRARY DESTINATION .
    COMPONENT python_module
)
