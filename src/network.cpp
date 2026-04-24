// network.cpp – standalone Asio version (no Boost libraries)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <asio.hpp>
#include <deque>
#include <mutex>
#include <thread>
#include <iostream>

#include "network.h"

using asio::ip::tcp;

// ---------- Internal client class ----------
class NetworkClient {
public:
    NetworkClient(asio::io_context& io, const std::string& host, unsigned short port)
        : socket_(io), io_context_(io), write_in_progress_(false)
    {
        tcp::resolver resolver(io);
        asio::connect(socket_, resolver.resolve(host, std::to_string(port)));
        start_read();
    }

    ~NetworkClient() {
        asio::error_code ec;
        socket_.close(ec);
    }

    void send(const std::string& msg)
    {
        bool was_empty;
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            was_empty = write_queue_.empty();
            write_queue_.push_back(msg + "\n");
        }
        if (was_empty && !write_in_progress_) {
            start_write();
        }
    }

    bool hasMessage()
    {
        std::lock_guard<std::mutex> lock(read_mutex_);
        return !read_queue_.empty();
    }

    std::string popMessage()
    {
        std::lock_guard<std::mutex> lock(read_mutex_);
        if (read_queue_.empty()) return "";
        std::string msg = read_queue_.front();
        read_queue_.pop_front();
        return msg;
    }

    bool isConnected() const
    {
        return socket_.is_open();
    }

private:
    tcp::socket socket_;
    asio::io_context& io_context_;
    asio::streambuf read_buffer_;

    std::deque<std::string> write_queue_;
    std::mutex write_mutex_;
    bool write_in_progress_;

    std::deque<std::string> read_queue_;
    std::mutex read_mutex_;

    void start_read()
    {
        asio::async_read_until(socket_, read_buffer_, '\n',
            [this](asio::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::istream is(&read_buffer_);
                    std::string line;
                    std::getline(is, line);
                    if (!line.empty()) {
                        std::lock_guard<std::mutex> lock(read_mutex_);
                        read_queue_.push_back(std::move(line));
                    }
                    start_read();
                } else {
                    socket_.close();
                }
            });
    }

    void start_write()
    {
        write_in_progress_ = true;
        std::string msg;
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            if (write_queue_.empty()) {
                write_in_progress_ = false;
                return;
            }
            msg = write_queue_.front();
        }
        asio::async_write(socket_, asio::buffer(msg),
            [this](asio::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    {
                        std::lock_guard<std::mutex> lock(write_mutex_);
                        write_queue_.pop_front();
                    }
                    write_in_progress_ = false;
                    if (!write_queue_.empty()) {
                        start_write();
                    }
                } else {
                    socket_.close();
                }
            });
    }
};

// ---------- Global state ----------
static asio::io_context* g_io_context = nullptr;
static NetworkClient* g_client = nullptr;
static std::thread* g_io_thread = nullptr;

// ---------- Public API implementation (same as before) ----------
bool init_network(const std::string& server_ip, unsigned short port)
{
    if (g_client) return true;

    try{
        g_io_context = new asio::io_context();
        g_client = new NetworkClient(*g_io_context, server_ip, port);
        g_io_thread = new std::thread([&]() { g_io_context->run(); });
        return true;
    }
    catch (const std::exception& e){
        std::cerr << "Network init failed: " << e.what() << std::endl;
        close_network();
        return false;
    }
}

void send_message(const std::string& msg)
{
    if (g_client && g_client->isConnected()){
        g_client->send(msg);
    }
}

bool has_message()
{
    return g_client && g_client->hasMessage();
}

std::string get_message()
{
    if (g_client) return g_client->popMessage();
    return "";
}

bool is_connected()
{
    return g_client && g_client->isConnected();
}

void close_network()
{
    if (g_io_thread){
        if (g_io_context) g_io_context->stop();
        if (g_io_thread->joinable()) g_io_thread->join();
        delete g_io_thread;
        g_io_thread = nullptr;
    }
    delete g_client;
    g_client = nullptr;
    delete g_io_context;
    g_io_context = nullptr;
}