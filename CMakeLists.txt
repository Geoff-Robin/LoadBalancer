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
    src/slam/connection_queue.cpp
    src/slam/router.cpp
    src/slam/server.cpp
)

target_include_directories(slam PUBLIC src)
target_link_libraries(slam PUBLIC Boost::boost nlohmann_json::nlohmann_json PRIVATE spdlog::spdlog)

add_library(requests src/requests/backend_connection_pool.cpp)
target_include_directories(requests PUBLIC src)
target_link_libraries(requests PUBLIC Boost::boost PRIVATE spdlog::spdlog)

add_library(core
    src/core/backend_endpoint.cpp
    src/core/backend_health_monitor.cpp
    src/core/backend_proxy.cpp
    src/core/backend_registry.cpp
    src/core/round_robin_strategy.cpp
)
target_include_directories(core PUBLIC src)
target_link_libraries(core PUBLIC Boost::boost PRIVATE requests spdlog::spdlog SQLite::SQLite3)

add_library(routes
    src/routes/backend_routes.cpp
    src/routes/health_routes.cpp
)
target_include_directories(routes PUBLIC src)
target_link_libraries(routes PUBLIC core slam)

add_executable(load_balancer src/proxy.cpp)

target_link_libraries(load_balancer PRIVATE requests routes spdlog::spdlog)

set(FORMAT_SOURCES
    src/proxy.cpp
    src/core/backend_registry.cpp
    src/core/backend_registry.hpp
    src/core/backend_endpoint.cpp
    src/core/backend_endpoint.hpp
    src/core/backend_health_monitor.cpp
    src/core/backend_health_monitor.hpp
    src/core/backend_proxy.cpp
    src/core/backend_proxy.hpp
    src/core/load_balancing_strategy.hpp
    src/core/round_robin_strategy.cpp
    src/core/round_robin_strategy.hpp
    src/requests/backend_connection_pool.cpp
    src/requests/backend_connection_pool.hpp
    src/routes/backend_routes.cpp
    src/routes/backend_routes.hpp
    src/routes/health_routes.cpp
    src/routes/health_routes.hpp
    src/slam/connection_queue.cpp
    src/slam/connection_queue.hpp
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
