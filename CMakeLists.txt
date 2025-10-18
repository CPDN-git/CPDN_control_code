cmake_minimum_required(VERSION 3.10)
project(control_code VERSION 1.0.0 LANGUAGES C CXX)

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

set(CPDNZIP_LIB_NAME "cpdn_zip")
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
add_library(control_code ./CPDN_control_code.cpp)
target_include_directories(control_code PUBLIC .)

# Add external header paths for boinc and cpdnzip
# These paths are PUBLIC because code needs them to compile.
target_include_directories(control_code PUBLIC 
    ${BOINC_INCLUDE_DIR}
    ${CPDNZIP_INCLUDE_DIR}
)

# Link the external libraries
# These are PRIVATE links, meaning only rcf_utility needs them directly for linking.
target_link_libraries(control_code PRIVATE
    ${BOINC_API} ${BOINC_LIB}
    ${CPDNZIP_LIB}
)

# Include the 'tests' subdirectory to process its CMakeLists.txt
add_subdirectory(utests)