# cmake/Version.cmake
# Automated Semantic Versioning and Metadata Extraction for NeoNect

# 1. Fallback / Default Semantic Version
set(NEONECT_VERSION_MAJOR 1)
set(NEONECT_VERSION_MINOR 0)
set(NEONECT_VERSION_PATCH 0)
set(NEONECT_VERSION_TWEAK 0)
set(NEONECT_VERSION_PRERELEASE "")

# 2. Check for Git repository and extract Git metadata
find_package(Git QUIET)

set(NEONECT_GIT_HASH "unknown")
set(NEONECT_GIT_BRANCH "main")
set(NEONECT_GIT_TAG "")

if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    # Get current commit hash (short)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NEONECT_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Get active branch name
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NEONECT_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Check for latest matching git tag (v*.*.*)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*" --abbrev=0
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NEONECT_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # If tag found, parse version numbers
    if(NEONECT_GIT_TAG MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)$")
        set(NEONECT_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(NEONECT_VERSION_MINOR ${CMAKE_MATCH_2})
        set(NEONECT_VERSION_PATCH ${CMAKE_MATCH_3})
        set(NEONECT_VERSION_PRERELEASE ${CMAKE_MATCH_4})
    endif()

    # Get commit count for build number
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NEONECT_BUILD_NUMBER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(NOT NEONECT_BUILD_NUMBER)
    set(NEONECT_BUILD_NUMBER "1")
endif()

# Environment variable overrides (e.g. from GitHub Actions CI/CD)
if(DEFINED ENV{NEONECT_VERSION})
    if("$ENV{NEONECT_VERSION}" MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)$")
        set(NEONECT_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(NEONECT_VERSION_MINOR ${CMAKE_MATCH_2})
        set(NEONECT_VERSION_PATCH ${CMAKE_MATCH_3})
        set(NEONECT_VERSION_PRERELEASE ${CMAKE_MATCH_4})
    endif()
endif()

# Construct semantic version strings
set(NEONECT_VERSION_STRING "${NEONECT_VERSION_MAJOR}.${NEONECT_VERSION_MINOR}.${NEONECT_VERSION_PATCH}")
if(NOT "${NEONECT_VERSION_PRERELEASE}" STREQUAL "")
    set(NEONECT_VERSION_STRING "${NEONECT_VERSION_STRING}${NEONECT_VERSION_PRERELEASE}")
endif()

set(NEONECT_VERSION_FULL "${NEONECT_VERSION_STRING}+${NEONECT_GIT_HASH}")

# Build timestamp (ISO 8601 UTC)
string(TIMESTAMP NEONECT_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)
string(TIMESTAMP NEONECT_BUILD_YEAR "%Y" UTC)

# Target platform name
if(WIN32)
    set(NEONECT_PLATFORM "Windows x86_64")
elseif(APPLE)
    set(NEONECT_PLATFORM "macOS")
elseif(UNIX)
    set(NEONECT_PLATFORM "Linux x86_64")
else()
    set(NEONECT_PLATFORM "Unknown")
endif()

message(STATUS "--------------------------------------------------------")
message(STATUS "➔ NeoNect Semantic Version : ${NEONECT_VERSION_STRING}")
message(STATUS "➔ NeoNect Full Version     : ${NEONECT_VERSION_FULL}")
message(STATUS "➔ Git Commit Hash          : ${NEONECT_GIT_HASH}")
message(STATUS "➔ Git Branch               : ${NEONECT_GIT_BRANCH}")
message(STATUS "➔ Build Number             : ${NEONECT_BUILD_NUMBER}")
message(STATUS "➔ Build Timestamp          : ${NEONECT_BUILD_TIMESTAMP}")
message(STATUS "➔ Target Platform          : ${NEONECT_PLATFORM}")
message(STATUS "--------------------------------------------------------")
