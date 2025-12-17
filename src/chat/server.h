#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <deque>
#include <string>

using boost::asio::ip::tcp;

// ---- 채팅 메시지 큐 타입 ----
using message = std::string;
using message_queue = std::deque<message>;

class chat_session;

class chat_room {
public:
    void join(std::shared_ptr<chat_session> session);
    void leave(std::shared_ptr<chat_session> session);
    void deliver(const message& msg);

private:
    std::set<std::shared_ptr<chat_session>> sessions_;
    std::mutex mutex_;
    // 최근 메시지 보관하고 싶다면 deque 유지 가능
};

class chat_session : public std::enable_shared_from_this<chat_session> {
public:
    explicit chat_session(tcp::socket socket, chat_room& room);

    void start();
    void deliver(const message& msg);

private:
    void do_read();
    void do_write();

    tcp::socket socket_;
    chat_room& room_;
    boost::asio::streambuf buffer_;
    message_queue write_msgs_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
};

class chat_server {
public:
    chat_server(boost::asio::io_context& io, const tcp::endpoint& ep);

private:
    void do_accept();

    tcp::acceptor acceptor_;
    chat_room room_;
};
