# Helper to configure the BOINC dependency for this project.
function(configure_boinc)
    set(BOINC_LIB_NAME "boinc")
    set(BOINC_API_NAME "boinc_api")

    set(
        BOINC_INCLUDE_DIR
        "../boinc-8.0.2-x86_64/include"
        CACHE PATH "Path to BOINC headers."
    )
    set(
        BOINC_LIB_DIR
        "../boinc-8.0.2-x86_64/lib"
        CACHE PATH "Path to BOINC libraries."
    )

    # Prefer the static BOINC libraries to satisfy the fully-static link.
    set(_BOINC_OLD_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    find_library(BOINC_LIB NAMES ${BOINC_LIB_NAME} HINTS ${BOINC_LIB_DIR})
    find_library(BOINC_API NAMES ${BOINC_API_NAME} HINTS ${BOINC_LIB_DIR})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_BOINC_OLD_SUFFIXES})

    if (NOT BOINC_LIB)
        message(
            FATAL_ERROR
            "Could not find BOINC library ${BOINC_LIB_NAME}. Check BOINC_LIB_DIR."
        )
    endif()

    if (NOT BOINC_API)
        message(
            FATAL_ERROR
            "Could not find BOINC API library ${BOINC_API_NAME}. Check BOINC_LIB_DIR."
        )
    endif()

    # Promote variables so the parent scope can use them when linking.
    set(BOINC_LIB ${BOINC_LIB} PARENT_SCOPE)
    set(BOINC_API ${BOINC_API} PARENT_SCOPE)
    set(BOINC_INCLUDE_DIR ${BOINC_INCLUDE_DIR} PARENT_SCOPE)
    set(BOINC_LIB_DIR ${BOINC_LIB_DIR} PARENT_SCOPE)
endfunction()
