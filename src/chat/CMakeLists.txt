add_executable(chat_server server.cpp server.h)

target_include_directories(chat_server PRIVATE /opt/homebrew/include)
target_include_directories(config PUBLIC config)
target_include_directories(cache PUBLIC cache)
find_package(spdlog REQUIRED)
target_link_libraries(chat_server PRIVATE spdlog::spdlog)
# db 라이브러리 + Boost 링크
target_link_libraries(chat_server PRIVATE
    db
    config
    cache
)

# Boost.System 라이브러리 제거
# target_link_libraries(chat_server PRIVATE /opt/homebrew/opt/boost/lib/libboost_system.dylib)

if(UNIX AND NOT APPLE)
  target_link_libraries(chat_server PRIVATE pthread)
endif()