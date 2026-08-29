cmake_minimum_required(VERSION 3.20)
project(LoadBalancer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(load_balancer proxy.cpp)

target_link_libraries(load_balancer PRIVATE Boost::boost)
