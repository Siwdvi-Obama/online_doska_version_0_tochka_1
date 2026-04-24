// g++ -std=c++11 server.cpp -o server.exe -I C:/asio/asio-1.36.0/include -DASIO_STANDALONE -D_WIN32_WINNT=0x0600 -lws2_32 -lmswsock

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "message_types.h"
#include <asio.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <set>
#include <deque>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

using asio::ip::tcp;

std::string int_to_hex(int value)
{
    std::ostringstream oss;
    oss << std::setw(10) << std::setfill('0') << value;
    return oss.str();
}

// Global counter for unique client IDs
static std::atomic<int> next_id{1};

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(tcp::socket socket, std::set<std::shared_ptr<ClientSession>>& sessions)
        : socket_(std::move(socket)), sessions_(sessions), id_(next_id++) {}

    void start() {
        sessions_.insert(shared_from_this());
        // Send welcome message to this client
        deliver((std::string(1, MSG_SEND_USERS_AMOUNT) + std::string(int_to_hex(sessions_.size())) + std::string(int_to_hex(id()))).data());
        //broadcast_count();

        read_message();
    }

    void deliver(const std::string& message) {
        bool write_in_progress = !write_queue_.empty();
        write_queue_.push_back(message + "\n");
        if (!write_in_progress) {
            write_message();
        }
    }

    int id() const { return id_; }

private:
    tcp::socket socket_;
    std::set<std::shared_ptr<ClientSession>>& sessions_;
    asio::streambuf read_buffer_;
    std::deque<std::string> write_queue_;
    int id_;

    void send_system_message(const std::string& text)
    {
        deliver("[SERVER] " + text);
    }

    void broadcast_system_message(const std::string& text)
    {
        for (auto& client : sessions_) {
            client->deliver(text);
        }
    }

    void broadcast_count()
    {
        int count = static_cast<int>(sessions_.size());
        std::string msg = "[SERVER] Users online: " + std::to_string(count);
        for (auto& client : sessions_) {
            client->deliver(msg);
        }
    }

    void read_message()
    {
        auto self = shared_from_this();
        asio::async_read_until(socket_, read_buffer_, '\n',
            [this, self](asio::error_code ec, std::size_t /*length*/) {
                if (!ec){
                    std::istream is(&read_buffer_);
                    std::string message;
                    std::getline(is, message);
                    if (!message.empty()) {
                        // Broadcast to all other clients
                        for (auto& client : sessions_){
                            if (client != self) {
                                client->deliver(message);
                            }
                        }
                    }
                    read_message();
                }
                else{
                    // Client disconnected – remove and announce
                    sessions_.erase(self);
                    broadcast_system_message((std::string(1, MSG_USER_LEAVE) + int_to_hex(id_)).data());
                    //broadcast_count();
                }
            });
    }

    void write_message() {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(write_queue_.front()),
            [this, self](asio::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_queue_.pop_front();
                    if (!write_queue_.empty()) {
                        write_message();
                    }
                } else {
                    sessions_.erase(self);
                }
            });
    }
};

class ChatServer {
public:
    ChatServer(asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        accept();
        std::cout << "version 0.4.1.0.0.000\n";
        std::cout << "Server running on port " << port << "\n";
    }

private:
    tcp::acceptor acceptor_;
    std::set<std::shared_ptr<ClientSession>> sessions_;

    void accept() {
        acceptor_.async_accept(
            [this](asio::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<ClientSession>(std::move(socket), sessions_);
                    session->start();
                }
                accept();
            });
    }
};

int main(int argc, char* argv[]) {
    short port = static_cast<short>(12345);

    try {
        asio::io_context io_context;
        ChatServer server(io_context, port);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}