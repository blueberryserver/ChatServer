# src/db/CMakeLists.txt
add_library(db STATIC
    AccountDb.cpp
    ShardDb.cpp
    DbRouter.cpp
    DbFacade.cpp
)

find_package(spdlog REQUIRED)
target_link_libraries(db PRIVATE spdlog::spdlog)

target_include_directories(db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# SOCI core + PostgreSQL 드라이버 직접 링크
target_link_libraries(db PUBLIC
    soci_core
    soci_postgresql
)