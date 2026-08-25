cmake_minimum_required(VERSION 3.20)
project(VirtualInputLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(VirtualInputTarget WIN32 src/target.cpp)
add_executable(VirtualInputController WIN32 src/controller.cpp)

foreach(target VirtualInputTarget VirtualInputController)
  target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(${target} PRIVATE comctl32)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  endif()
endforeach()

