cmake_minimum_required(VERSION 3.20)
project(AutoSyncClean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(V81_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(V81_GENERATED_MAIN "${V81_GENERATED_DIR}/main_v81.cpp")
file(MAKE_DIRECTORY "${V81_GENERATED_DIR}")

execute_process(
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/make_v79_source.ps1"
            -InputPath "${CMAKE_SOURCE_DIR}/src/main.cpp"
            -OutputPath "${V81_GENERATED_MAIN}"
    RESULT_VARIABLE V81_PATCH_RESULT
    OUTPUT_VARIABLE V81_PATCH_STDOUT
    ERROR_VARIABLE V81_PATCH_STDERR
)
if(NOT V81_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to generate v81 source.\n${V81_PATCH_STDOUT}\n${V81_PATCH_STDERR}")
endif()
message(STATUS "${V81_PATCH_STDOUT}")

add_executable(AutoSyncClean WIN32
    "${V81_GENERATED_MAIN}"
    src/app.rc
    src/app.manifest
)

set_target_properties(AutoSyncClean PROPERTIES OUTPUT_NAME "AutoSync Clean v81")

target_include_directories(AutoSyncClean PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(AutoSyncClean PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(AutoSyncClean PRIVATE comctl32 dwmapi comdlg32 advapi32)

if(MSVC)
    target_compile_options(AutoSyncClean PRIVATE /W4 /permissive- /utf-8)
    target_link_options(AutoSyncClean PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
