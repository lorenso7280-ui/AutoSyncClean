cmake_minimum_required(VERSION 3.20)
project(AutoSyncClean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(V79_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(V79_GENERATED_MAIN "${V79_GENERATED_DIR}/main_v79.cpp")
file(MAKE_DIRECTORY "${V79_GENERATED_DIR}")

execute_process(
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/make_v79_source.ps1"
            -InputPath "${CMAKE_SOURCE_DIR}/src/main.cpp"
            -OutputPath "${V79_GENERATED_MAIN}"
    RESULT_VARIABLE V79_PATCH_RESULT
    OUTPUT_VARIABLE V79_PATCH_STDOUT
    ERROR_VARIABLE V79_PATCH_STDERR
)
if(NOT V79_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to generate v79 source.\n${V79_PATCH_STDOUT}\n${V79_PATCH_STDERR}")
endif()
message(STATUS "${V79_PATCH_STDOUT}")

add_executable(AutoSyncClean WIN32
    "${V79_GENERATED_MAIN}"
    src/app.rc
    src/app.manifest
)

set_target_properties(AutoSyncClean PROPERTIES OUTPUT_NAME "AutoSyncClean v.79 Clean")

target_include_directories(AutoSyncClean PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(AutoSyncClean PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(AutoSyncClean PRIVATE comctl32 dwmapi comdlg32 advapi32)

if(MSVC)
    target_compile_options(AutoSyncClean PRIVATE /W4 /permissive- /utf-8)
    target_link_options(AutoSyncClean PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
