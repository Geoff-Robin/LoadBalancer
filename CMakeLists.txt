cmake_minimum_required(VERSION 3.20)
project(LoadBalancer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

add_library(slam
    src/slam/router.cpp
    src/slam/server.cpp
)

target_include_directories(slam PUBLIC src)
target_link_libraries(slam PUBLIC Boost::boost nlohmann_json::nlohmann_json PRIVATE spdlog::spdlog)

add_library(core src/core/backend_registry.cpp)
target_include_directories(core PUBLIC src)
target_link_libraries(core PRIVATE spdlog::spdlog SQLite::SQLite3)

add_executable(load_balancer src/proxy.cpp)

target_link_libraries(load_balancer PRIVATE core slam spdlog::spdlog)

set(FORMAT_SOURCES
    src/proxy.cpp
    src/core/backend_registry.cpp
    src/core/backend_registry.hpp
    src/slam/router.cpp
    src/slam/router.hpp
    src/slam/server.cpp
    src/slam/server.hpp
)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting C++ source files"
    )
else()
    message(STATUS "clang-format not found; the format target is unavailable")
endif()
