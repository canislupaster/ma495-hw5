cmake_minimum_required(VERSION 3.29)
project(mathmodel CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_COLOR_DIAGNOSTICS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Wextra -Wshadow -Wno-sign-compare)

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(-O3)
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(BUILD_DEBUG)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif ()

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

find_package(GeographicLib REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CairoMM REQUIRED IMPORTED_TARGET cairo cairomm-1.16)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE PkgConfig::CairoMM nlohmann_json::nlohmann_json ${GeographicLib_LIBRARIES})
target_include_directories(main PRIVATE ${GeographicLib_INCLUDE_DIRS})