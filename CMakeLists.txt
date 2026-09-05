# SMPD - Steam Multi Pattern Downloader (winmm.dll proxy)
#   cmake --preset x64-Release
#   cmake --build --preset x64-Release
# Release x64 only. Dependencies via FetchContent (toml++ only).
# Output: out/Release/winmm.dll (copy next to steam.exe)

cmake_minimum_required(VERSION 3.20)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Floor for old dependency CMakeLists")

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Static runtime (avoids a VCRedist dependency and
# one class of 0xc000007b caused by a missing CRT on the target).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# VERSION — single source of truth.
# The number lands inside winmm.dll as VS_VERSION_INFO.
project(SMPD LANGUAGES C CXX RC VERSION 0.1.0)

if(NOT WIN32)
    message(FATAL_ERROR "SMPD targets Windows only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SMPD is x64-only. Select the x64 architecture/preset.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- version resource generator ---
set(SMPD_PRODUCT_VERSION "${PROJECT_VERSION}")
string(REPLACE "." "," SMPD_FILEVERSION_COMMA "${PROJECT_VERSION}.0")

function(attach_version_resource TARGET_NAME INTERNAL_NAME ORIGINAL_FILENAME DESCRIPTION)
    set(SMPD_RC_INTERNAL_NAME "${INTERNAL_NAME}")
    set(SMPD_RC_ORIGINAL_FILENAME "${ORIGINAL_FILENAME}")
    set(SMPD_RC_FILEDESCRIPTION "${DESCRIPTION}")
    set(_smpd_version_rc "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_version.rc")
    configure_file("${CMAKE_SOURCE_DIR}/common/version.rc.in" "${_smpd_version_rc}" @ONLY)
    target_sources(${TARGET_NAME} PRIVATE "${_smpd_version_rc}")
endfunction()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Only Release builds are allowed for SMPD. Use the x64-Release preset.")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

include(FetchContent)

# toml++ (only used to count/parse TOML)
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0)
FetchContent_MakeAvailable(tomlplusplus)

add_subdirectory(SmpdProxy)

set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")
install(DIRECTORY ${OUTPUT_DIR}/ DESTINATION bin COMPONENT Runtime)
