#  CMakeLists.txt for cpdn_controller
#
#  Sets up the build configuration for the CPDN Controller app,
#  and linking against the BOINC and CPDNZIP libraries.
#
#  Currently only runs the CTest unit tests in the 'utests' subdirectory.
#  Will be extended later to build the full application.
#
#      Glenn Carver, CPDN, Oct, 2025.


cmake_minimum_required(VERSION 3.10)
project(cpdn_control VERSION 1.0.0 LANGUAGES C CXX)

# Require C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- CONFIGURATION FOR BOINC LIBRARY

set(BOINC_LIB_NAME "boinc")
set(BOINC_API_NAME "boinc_api")

set(BOINC_INCLUDE_DIR "../boinc-8.0.2-x86_64/include" CACHE PATH "Path to BOINC headers.")
set(BOINC_LIB_DIR     "../boinc-8.0.2-x86_64/lib"     CACHE PATH "Path to BOINC libraries.")

# Locate the pre-built BOINC libraries
find_library(BOINC_LIB  NAMES ${BOINC_LIB_NAME}  HINTS ${BOINC_LIB_DIR} )
find_library(BOINC_API  NAMES ${BOINC_API_NAME}  HINTS ${BOINC_LIB_DIR} )

if (NOT BOINC_LIB)
    message(FATAL_ERROR "Could not find BOINC library ${BOINC_LIB_NAME}. Check BOINC_LIB_DIR.")
endif()


# --- CONFIGURATION FOR CPDNZIP LIBRARY (Headers & Lib)

set(CPDNZIP_LIB_NAME    "cpdn_zip")
set(CPDNZIP_INCLUDE_DIR "zip/install/include" CACHE PATH "Path to CPDNZIP library headers.")
set(CPDNZIP_LIB_DIR     "zip/install/lib"     CACHE PATH "Path to CPDNZIP library files.")

find_library(CPDNZIP_LIB  NAMES ${CPDNZIP_LIB_NAME}  HINTS ${CPDNZIP_LIB_DIR} )

if (NOT CPDNZIP_LIB)
    message(FATAL_ERROR "Could not find CPDNZIP library ${CPDNZIP_LIB_NAME}. Check CPDNZIP_LIB_DIR.")
endif()

# ----------------------------------------------------

#  Enable CTest support for the project
enable_testing()

# Add the source so tests can link against it
add_library(cpdn_control 
    ./src/cpdn_control.cpp
    ./lib/utils.cpp
    ./lib/cpdn_linux_cpu_time.cpp
    ./api/trickle_handler.cpp
)
target_include_directories(cpdn_control PUBLIC 
    ./
    ./src
    ./lib
    ./api
)

# Add external header paths for boinc and cpdnzip
# These paths are PUBLIC because code needs them to compile.
target_include_directories(cpdn_control PUBLIC 
    ${BOINC_INCLUDE_DIR}
    ${CPDNZIP_INCLUDE_DIR}
)

# Link the external libraries
# These are PRIVATE links, meaning only rcf_utility needs them directly for linking.
target_link_libraries(cpdn_control PRIVATE
    ${BOINC_API} ${BOINC_LIB}
    ${CPDNZIP_LIB}
)

# Include the 'tests' subdirectory to process its CMakeLists.txt
add_subdirectory(utests)
