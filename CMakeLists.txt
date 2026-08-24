cmake_minimum_required(VERSION 3.20)
project(AutoSyncClean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AutoSyncClean WIN32
    src/main.cpp
    src/app.rc
)

set_target_properties(AutoSyncClean PROPERTIES OUTPUT_NAME "AutoSyncClean v.66")

target_compile_definitions(AutoSyncClean PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(AutoSyncClean PRIVATE comctl32 dwmapi comdlg32 advapi32)

if(MSVC)
    target_compile_options(AutoSyncClean PRIVATE /W4 /permissive- /utf-8)
    target_link_options(AutoSyncClean PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
