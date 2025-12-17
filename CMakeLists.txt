cmake_minimum_required(VERSION 3.15)
project(mmorpg-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 공통 include/lib 경로 (Homebrew + SOCI 빌드 경로)
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)


# 하위 디렉토리 추가
add_subdirectory(external/config)
add_subdirectory(external/cache)
add_subdirectory(src/db)
add_subdirectory(src/chat)
# add_subdirectory(src/gateway)
# add_subdirectory(src/zone)
# add_subdirectory(src/quest)