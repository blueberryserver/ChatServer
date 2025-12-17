cmake_minimum_required(VERSION 3.14)
project(cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 라이브러리 생성
add_library(cache src/RedisClient.cpp)
target_include_directories(cache PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(cache PUBLIC /opt/homebrew/Cellar/hiredis/1.3.0/include)
target_link_libraries(cache PUBLIC /usr/local/lib/libredis++.dylib /opt/homebrew/Cellar/hiredis/1.3.0/lib/libhiredis.dylib)
