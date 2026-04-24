// drawing_server.cpp
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <set>
#include <mutex>
#include <iostream>
#include <queue>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

class Session;
using SessionPtr = std::shared_ptr<Session>;

class DrawingServer {
private:
    asio::io_context io_context_;
    tcp::acceptor acceptor_;
    std::set<SessionPtr> sessions_;
    std::mutex sessions_mutex_;
    
    // Drawing state (optional - for syncing new clients)
    std::vector<std::string> drawing_history_;
    std::mutex history_mutex_;

public:
    DrawingServer(short port)
        : io_context_(1)
        , acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "Drawing Server started on port " << port << std::endl;
        start_accept();
    }

    void run() {
        io_context_.run();
    }

private:
    void start_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "New client connected" << std::endl;
                    auto session = std::make_shared<Session>(
                        std::move(socket), 
                        *this
                    );
                    {
                        std::lock_guard<std::mutex> lock(sessions_mutex_);
                        sessions_.insert(session);
                    }
                    session->start();
                }
                start_accept();
            }
        );
    }

    void broadcast(const std::string& message, SessionPtr exclude = nullptr) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_) {
            if (session != exclude && session->is_open()) {
                session->send(message);
            }
        }
    }

    void add_to_history(const std::string& drawing_data) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        drawing_history_.push_back(drawing_data);
        
        // Keep last 1000 drawings
        if (drawing_history_.size() > 1000) {
            drawing_history_.erase(drawing_history_.begin());
        }
    }

    std::vector<std::string> get_history() {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return drawing_history_;
    }

    void remove_session(SessionPtr session) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
        std::cout << "Client disconnected. Remaining: " 
                  << sessions_.size() << std::endl;
    }

    friend class Session;
};

class Session : public std::enable_shared_from_this<Session> {
private:
    websocket::stream<tcp::socket> ws_;
    DrawingServer& server_;
    beast::flat_buffer buffer_;
    std::queue<std::string> write_queue_;
    bool writing_ = false;

public:
    Session(tcp::socket socket, DrawingServer& server)
        : ws_(std::move(socket)), server_(server) {}

    void start() {
        ws_.set_option(websocket::stream_base::timeout::suggested(
            beast::role_type::server));
        
        ws_.async_accept(
            beast::bind_front_handler(
                &Session::on_accept,
                shared_from_this()
            )
        );
    }

    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "Accept error: " << ec.message() << std::endl;
            return;
        }

        // Send existing drawing history to new client
        auto history = server_.get_history();
        for (const auto& drawing : history) {
            send(drawing);
        }

        do_read();
    }

    void do_read() {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &Session::on_read,
                shared_from_this()
            )
        );
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec == websocket::error::closed) {
            server_.remove_session(shared_from_this());
            return;
        }

        if (ec) {
            std::cerr << "Read error: " << ec.message() << std::endl;
            return;
        }

        // Process the drawing data
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        // Add to server history
        server_.add_to_history(message);
        
        // Broadcast to all other clients
        server_.broadcast(message, shared_from_this());

        // Continue reading
        do_read();
    }

    void send(const std::string& message) {
        // Queue the message for sending
        write_queue_.push(message);
        
        // If not already writing, start the write chain
        if (!writing_) {
            do_write();
        }
    }

    void do_write() {
        if (write_queue_.empty()) {
            writing_ = false;
            return;
        }

        writing_ = true;
        auto message = write_queue_.front();
        write_queue_.pop();

        ws_.async_write(
            asio::buffer(message),
            beast::bind_front_handler(
                &Session::on_write,
                shared_from_this()
            )
        );
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec) {
            std::cerr << "Write error: " << ec.message() << std::endl;
            return;
        }

        // Continue writing if there are more messages
        do_write();
    }

    bool is_open() const {
        return ws_.is_open();
    }
};

int main(int argc, char* argv[]) {
    try {
        short port = 9002;
        if (argc == 2) {
            port = std::atoi(argv[1]);
        }
        
        DrawingServer server(port);
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}