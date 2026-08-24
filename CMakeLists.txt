cmake_minimum_required(VERSION 3.15)
project(MiPlugin_Traff VERSION 1.1.2 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制使用 UTF-8 与静态 CRT (/MT)
if (MSVC)
    add_compile_options(/utf-8 /W3)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_options("$<$<CONFIG:Release>:/GL;/Gy;/GF;/O2>")
endif()

include_directories(include)

set(SOURCES
    src/XiaomiPlugPlugin.cpp
    src/MiioClient.cpp
    src/ConfigManager.cpp
    src/OptionsDlg.cpp
    res/Version.rc
)

add_library(XiaomiPlugPlugin SHARED ${SOURCES})

target_link_libraries(XiaomiPlugPlugin PRIVATE
    ws2_32
    bcrypt
    user32
    gdi32
    comctl32
    advapi32
)

set_target_properties(XiaomiPlugPlugin PROPERTIES
    OUTPUT_NAME "XiaomiPlugPlugin"
    PREFIX ""
)

if (MSVC)
    target_link_options(XiaomiPlugPlugin PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()
