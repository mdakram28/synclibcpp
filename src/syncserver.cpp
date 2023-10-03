#include <algorithm>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <set>
#include "diff.hpp"
#include "statevar.hpp"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": (" << ec << ") " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::string session_id;
    bool is_closed_ = false;
    OnDiffReceiveCallback callback;

   public:
    // Take ownership of the socket
    explicit Session(tcp::socket&& socket, std::string session_id, OnDiffReceiveCallback callback)
        : ws_(std::move(socket)), session_id(session_id), callback(callback) {}

    bool is_closed() { return is_closed_; }

    bool send_diff(StateDiff& diff) {
        // Get encoded data
        std::string data = diff.to_string();

        // Write to buffer
        buffer_.consume(buffer_.size());
        memcpy(buffer_.prepare(data.size()).data(), data.data(), data.size());
        buffer_.commit(data.size());

        // Send
        ws_.async_write(buffer_.data(), beast::bind_front_handler(&Session::on_write, shared_from_this()));

        return true;
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());
    }

    // Get on the correct executor
    void run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(ws_.get_executor(), beast::bind_front_handler(&Session::on_run, shared_from_this()));
    }

    // Start the asynchronous operation
    void on_run() {
        // Set suggested timeout settings for the websocket
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
        }));
        // Accept the websocket handshake
        ws_.async_accept(beast::bind_front_handler(&Session::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void do_read() {
        buffer_.consume(buffer_.size());
        // Read a message into our buffer
        ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == websocket::error::closed || ec == boost::asio::error::eof) {
            is_closed_ = true;
            return;
        }

        if (ec)
            return fail(ec, "read");

        // Echo the message
        ws_.text(ws_.got_text());
        StateDiff diff;
        std::string err_msg;
        std::string message = beast::buffers_to_string(buffer_.data());
        std::cout << "Received message: " << message << std::endl;

        StateDiff::from_string(message, diff, err_msg);

        callback(session_id, diff);
        do_read();
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WsStateServer : public std::enable_shared_from_this<WsStateServer>, public StateTransport {
   public:
    uint8_t num_threads;
    net::io_context ioc;
    tcp::acceptor acceptor;
    tcp::endpoint endpoint;
    uint32_t session_counter;
    std::map<std::string, std::shared_ptr<Session>> peers;
    std::vector<std::thread> threads;

    WsStateServer(uint8_t num_threads, tcp::endpoint endpoint)
        : num_threads(num_threads), ioc{num_threads}, acceptor(ioc), endpoint(endpoint) {}

    bool send_diff(const std::string& peer_id, StateDiff& diff) {
        if (peers.find(peer_id) == peers.end()) {
            std::cout << "Session not found : " << peer_id << std::endl;
            return false;
        }

        auto const peer = peers[peer_id];
        if (peer->is_closed()) {
            std::cout << "Session is closed, erasing : " << peer_id << std::endl;
            peers.erase(peer_id);
            return false;
        }

        return peer->send_diff(diff);
    }

    std::vector<std::string> get_peers() {
        std::vector<std::string> peer_ids;
        
        for (auto &pair: peers) {
            peer_ids.push_back(pair.first);
        }

        return std::move(peer_ids);
    }

    void start_server() {
        beast::error_code ec;

        // Open the acceptor
        acceptor.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }

        do_accept();

        for (auto i = 1; i <= num_threads; i++) {
            this->threads.emplace_back([&] {
                std::cout << "Starting tcp thread " << i << std::endl;
                this->ioc.run();
            });
        }
    }

   private:
    void do_accept() {
        // The new connection gets its own strand
        acceptor.async_accept(net::make_strand(ioc),
                              beast::bind_front_handler(&WsStateServer::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            fail(ec, "accept");
        } else {
            // Create the session and run it
            std::string sid = std::string("CLIENT_") + std::to_string(++session_counter);
            std::cout << "New Connection : " << sid << std::endl;
            std::shared_ptr<Session> session =
                std::make_shared<Session>(std::move(socket), sid,
                                          std::bind(&WsStateServer::diff_received, shared_from_this(),
                                                    std::placeholders::_1, std::placeholders::_2));
            this->peers[sid] = session;
            session->run();
        }

        // Accept another connection
        do_accept();
    }

    void diff_received(const std::string& peer_id, StateDiff& diff) {
        for (auto &listener : listeners) {
            listener.second(peer_id, diff);
        }
    }
};