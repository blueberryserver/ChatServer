#include <mutex>
#include "server.h"
#include <iostream>
#include <spdlog/spdlog.h>
// DB 추가
#include "../db/DbFacade.h"
#include "ConfigManager.h"
#include "RedisClient.h"
#include "ConfigTypes.h"

DbFacade g_db("dbname=account_db user=root password=password host=localhost");

cache::RedisClient g_cache(cache::RedisConfig{ .url = "tcp://127.0.0.1:6379" });
// chat_session 구현

chat_session::chat_session(tcp::socket socket, chat_room& room)
    : socket_(std::move(socket)), room_(room), strand_(socket_.get_executor()) {}

void chat_session::start() {
    room_.join(shared_from_this());
    do_read();
}

void chat_session::deliver(const message& msg) {
    auto self = shared_from_this();
    boost::asio::post(strand_, [this, self, msg] {
        bool writing = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!writing) do_write();
    });
}

void chat_session::do_read() {
    auto self = shared_from_this();
    boost::asio::async_read_until(socket_, buffer_, '\n',
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string line;
                    std::getline(is, line);
                    std::cout << line << std::endl;
                    SPDLOG_INFO("{}", line);

                    if (!line.empty()) {
                        // === DB 저장 예시 ===
                        // 유저 이름은 임시 "Alice" 로 가정 (실제로는 로그인 로직 필요)
                        auto user = g_db.findUser("Alice");
                        if (user) {
                            g_db.saveMessage(user->id, 1 /*room_id*/, line);
                        } else {
                            SPDLOG_WARN("not found user");
                            
                        }

                        // 채팅방 브로드캐스트
                        room_.deliver(line + "\n");
                    }
                    do_read();
                } else {
                    room_.leave(self);
                }
            }));
}

void chat_session::do_write() {
    auto self = shared_from_this();
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().size()),
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty()) do_write();
                } else {
                    SPDLOG_INFO("leave room");
                    room_.leave(self);
                }
            }));
}

// chat_room 구현

void chat_room::join(std::shared_ptr<chat_session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.insert(session);
    session->deliver("Welcome to the chat!\n");
}

void chat_room::leave(std::shared_ptr<chat_session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session);
}

void chat_room::deliver(const message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : sessions_) s->deliver(msg);
}

// chat_server 구현

chat_server::chat_server(boost::asio::io_context& io, const tcp::endpoint& ep)
    : acceptor_(io, ep) {
    do_accept();
}

void chat_server::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<chat_session>(std::move(socket), room_)->start();
            }
            SPDLOG_INFO("accept client");
            do_accept();
        });
}

int main(int argc, char* argv[]) {
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%s:%# %!] %v");

    config::ConfigManager cfg;

    if(cfg.load("../config.yaml")) {

        ServerConfig srvCfg = cfg.getStruct<ServerConfig>("server");

        SPDLOG_INFO("YAML server.host={}", srvCfg.host);
        SPDLOG_INFO("YAML server.port={}", srvCfg.port);

        DatabaseConfig dbCfg = cfg.getStruct<DatabaseConfig>("database");

        SPDLOG_INFO("YAML database.host={}", dbCfg.host);
        SPDLOG_INFO("YAML database.port={}", dbCfg.port);
        SPDLOG_INFO("YAML database.dbname={}", dbCfg.dbname);

        RedisConfig redisCfg = cfg.getStruct<RedisConfig>("redis");

        SPDLOG_INFO("YAML redis.url={}", redisCfg.url);
        SPDLOG_INFO("YAML redis.pool_size={}", redisCfg.pool_size);

        g_cache.Set("chat_server", "hahaha");
        auto value = g_cache.Get("chat_server");
        if (value) {
            SPDLOG_INFO("cache test: {}", *value);
        }
    }

    try {

        //SPDLOG_INFO("Hello, spdlog! number={}", 42);
        //SPDLOG_WARN("This is a warning!");
        //SPDLOG_ERROR("Something went wrong: {}", "error details");


        unsigned short port = 12345;
        if (argc >= 2) port = static_cast<unsigned short>(std::stoi(argv[1]));

        boost::asio::io_context io;
        chat_server server(io, tcp::endpoint(tcp::v4(), port));

        std::vector<std::thread> threads;
        for (unsigned i = 1; i < std::thread::hardware_concurrency(); ++i) {
            SPDLOG_INFO("io thread start");
            threads.emplace_back([&]{ io.run(); });
        }

        SPDLOG_INFO("Chat server started on port {}", port);
        io.run();
    } catch (std::exception& e) {
        SPDLOG_ERROR("exception: {}", e.what());
    }
}