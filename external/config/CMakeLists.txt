cmake_minimum_required(VERSION 3.14)
project(config LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 외부 의존성
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)

# 라이브러리 생성
add_library(config src/ConfigManager.cpp)
target_include_directories(config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(config PUBLIC nlohmann_json::nlohmann_json yaml-cpp)
