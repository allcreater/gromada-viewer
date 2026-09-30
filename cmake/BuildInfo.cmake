# Module for extracting build information (commit hash and build date)
# This follows industry best practices for embedding build metadata

function(setup_build_info TARGET_NAME)
    # Get git commit hash
    find_package(Git QUIET)
    set(GIT_COMMIT_HASH "unknown")
    
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    
    # Get build date and time (ISO 8601 format)
    string(TIMESTAMP BUILD_TIMESTAMP $<STRING:TIMESTAMP>)
    
    # Allow override via environment variables or CMake cache (for CI/CD systems)
    if(DEFINED ENV{BUILD_COMMIT_HASH})
        set(GIT_COMMIT_HASH $ENV{BUILD_COMMIT_HASH})
    endif()

    if(DEFINED ENV{BUILD_TIMESTAMP})
        set(BUILD_TIMESTAMP $ENV{BUILD_TIMESTAMP})
    endif()
    
    # Add compile definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE
        BUILD_INFO_COMMIT_HASH="${GIT_COMMIT_HASH}"
        BUILD_INFO_COMMIT_SHORT="$<STRING:SUBSTRING,${GIT_COMMIT_HASH}, 0, 7>"
        BUILD_INFO_TIMESTAMP="${BUILD_TIMESTAMP}"
        BUILD_INFO_PROJECT_VERSION="${PROJECT_VERSION}"
    )
    
    if(WIN32)
        add_windows_version_info(${TARGET_NAME} "${GIT_COMMIT_HASH}")
    endif()

    message(STATUS "Build Info:")
    message(STATUS "  Commit (full): ${GIT_COMMIT_HASH}")
    message(STATUS "  Build date: ${BUILD_TIMESTAMP}")
    message(STATUS "  Version: ${PROJECT_VERSION}")
endfunction()

function(add_windows_version_info TARGET_NAME COMMIT_HASH)
    foreach(component MAJOR MINOR PATCH TWEAK)
        if(NOT PROJECT_VERSION_${component})
            set(PROJECT_VERSION_${component} 0)
        endif()
    endforeach()

    set(VERSION_INFO_AUTHOR "Anton Semenov")
    string(TIMESTAMP VERSION_INFO_YEAR "%Y")
    string(SUBSTRING "${COMMIT_HASH}" 0 7 VERSION_INFO_COMMIT_SHORT)

    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_version_info.rc")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/version_info.rc.in" "${RC_FILE}" @ONLY)

    enable_language(RC)
    target_sources(${TARGET_NAME} PRIVATE "${RC_FILE}")
endfunction()
