# SmpdProxy - winmm.dll proxy + pattern bridge.
# Full forward generated from the real export table (winmm.def +
# winmm_stubs.asm MASM + winmm_forward.cpp). LumaCore stays untouched:
# LC's own dwmapi.dll/xinput1_4.dll remain in place.
# Output: winmm.dll -> <Steam>\winmm.dll
# Requires MASM (ml64, included with the C++ Build Tools).

project(SmpdProxy LANGUAGES CXX ASM_MASM)

add_library(SmpdProxy SHARED
    core/dllmain.cpp
    core/BridgeCache.cpp
    core/Logger.cpp
    network/RuntimeHttp.cpp
    utils/Hasher.cpp
    utils/PatternSource.cpp
    utils/PatternDownloader.cpp
    winmm_forward.cpp
    winmm_stubs.asm
    winmm.def
    pch.cpp
)

target_include_directories(SmpdProxy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
)

target_precompile_headers(SmpdProxy PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/common/pch.h>
)

target_link_libraries(SmpdProxy PRIVATE
    tomlplusplus::tomlplusplus
    bcrypt
    winhttp
    shlwapi
)

target_compile_definitions(SmpdProxy PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    SMPD_RELEASE
)

if(MSVC)
    # CXX only: ml64 would not understand /bigobj /utf-8 /EHsc.
    target_compile_options(SmpdProxy PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/bigobj>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
endif()

set_target_properties(SmpdProxy PROPERTIES
    OUTPUT_NAME "winmm"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out/Release"
)

attach_version_resource(SmpdProxy "SMPD" "winmm.dll" "SMPD Pattern Bridge")
