#  CMakeLists.txt for cpdn_controller
#
#  Sets up the build configuration for the CPDN Controller app,
#  and linking against the BOINC and CPDNZIP libraries.
#
#  Steps:
#    1. mkdir build (or remove it for fresh build).
#    2. cd build
#    3. cmake ..
#    4. cmake --build . --verbose (omit --verbose if not interested in compile commands)
#    5. ctest -V    (run unit tests, -V is verbose, optional)
#
#    Final executables can be found in the 'build' subdir.
#
#      Glenn Carver, CPDN, Oct, 2025.


cmake_minimum_required(VERSION 3.10)

project(cpdn_control VERSION 1.0.0 LANGUAGES C CXX)

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Version string and compiler flag configuration (mirrors the Makefile variables)
set(APP_VERSION "1.00"                 CACHE STRING "Version string appended to the executable names.")
set(PLATFORM    "x86_64-pc-linux-gnu"  CACHE_STRING "Platform and operating system.")
set(RELEASE_OUTPUT_NAME "cpdn_control_${APP_VERSION}_${PLATFORM}")
set(DEBUG_OUTPUT_NAME   "cpdn_control_${APP_VERSION}_${PLATFORM}-debug")

set(TARGET_COMPILE_OPTIONS -g -pthread -Wall CACHE STRING "Compile options for release target.")
set(TARGET_LINK_OPTIONS    -static           CACHE STRING "Linker options for release target.")
set(DEBUG_COMPILE_OPTIONS  -fsanitize=address -ggdb3 -pthread -Wall CACHE STRING "Compile options for debug target.")
set(DEBUG_LINK_OPTIONS     -fsanitize=address                       CACHE STRING "Linker options for debug target.")

include(cmake/GitHelpers.cmake)
get_git_hash(CODE_VERSION_HASH)

set(RELEASE_TARGET_NAME cpdn_release)
set(DEBUG_TARGET_NAME   cpdn_debug)

# --- CONFIGURATION FOR BOINC LIBRARY
include(cmake/BoincConfig.cmake)
configure_boinc()

# --- CONFIGURATION FOR CPDNZIP LIBRARY (Headers & Lib)
include(cmake/CPDNzipConfig.cmake)
configure_cpdnzip()

find_package(Threads REQUIRED)

# ----------------------------------------------------

#  Enable CTest support for the project
enable_testing()

# Add the source so tests can link against it
set(LOCAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/api
)

add_library(cpdn_control 
    ./src/cpdn_control.cpp
    ./lib/utils.cpp
    ./lib/cpdn_linux_cpu_time.cpp
    ./api/trickle_handler.cpp
)
target_include_directories(cpdn_control PUBLIC ${LOCAL_INCLUDE_DIRS})

# Add external header paths for boinc and cpdnzip
# These paths are PUBLIC because code needs them to compile.
target_include_directories(cpdn_control PUBLIC 
    ${BOINC_INCLUDE_DIR}
    ${CPDNZIP_INCLUDE_DIR}
)

# Link the external libraries
# Marked PUBLIC so downstream executables inherit the requirement.
target_link_libraries(cpdn_control PUBLIC
    ${BOINC_API} ${BOINC_LIB}
    ${CPDNZIP_LIB}
)

target_compile_definitions(cpdn_control PUBLIC CODE_VERSION="${CODE_VERSION_HASH}")

add_executable(${RELEASE_TARGET_NAME}
    ./src/openifs.cpp
)
set_target_properties(${RELEASE_TARGET_NAME} PROPERTIES OUTPUT_NAME ${RELEASE_OUTPUT_NAME})
target_link_libraries(${RELEASE_TARGET_NAME} PRIVATE cpdn_control Threads::Threads)
if(TARGET_COMPILE_OPTIONS)
    target_compile_options(${RELEASE_TARGET_NAME} PRIVATE ${TARGET_COMPILE_OPTIONS})
endif()
if(TARGET_LINK_OPTIONS)
    target_link_options(${RELEASE_TARGET_NAME} PRIVATE ${TARGET_LINK_OPTIONS})
endif()

add_executable(${DEBUG_TARGET_NAME}
    ./src/openifs.cpp
)
set_target_properties(${DEBUG_TARGET_NAME} PROPERTIES OUTPUT_NAME ${DEBUG_OUTPUT_NAME})
target_link_libraries(${DEBUG_TARGET_NAME} PRIVATE cpdn_control Threads::Threads)
if(DEBUG_COMPILE_OPTIONS)
    target_compile_options(${DEBUG_TARGET_NAME} PRIVATE ${DEBUG_COMPILE_OPTIONS})
endif()
if(DEBUG_LINK_OPTIONS)
    target_link_options(${DEBUG_TARGET_NAME} PRIVATE ${DEBUG_LINK_OPTIONS})
endif()

# Include the 'tests' subdirectory to process its CMakeLists.txt
add_subdirectory(utests)
