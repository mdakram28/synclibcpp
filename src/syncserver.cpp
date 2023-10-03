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
#include "statevar.hpp";

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class Session : public std::enable_shared_from_this<Session>, public StateVar {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    uint32_t session_id;

   public:
    // Take ownership of the socket
    explicit Session(tcp::socket&& socket, uint32_t session_id)
        : ws_(std::move(socket)), session_id(session_id) {
    }

    // Get on the correct executor
    void
    run() {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(ws_.get_executor(),
                      beast::bind_front_handler(
                          &Session::on_run,
                          shared_from_this()));
    }

    // Start the asynchronous operation
    void
    on_run() {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-server-async");
            }));
        // Accept the websocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &Session::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec) {
        if (ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read() {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &Session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == websocket::error::closed)
            return;

        if (ec)
            fail(ec, "read");

        // Echo the message
        ws_.text(ws_.got_text());

        std::cout << "Received message: " << beast::buffers_to_string(buffer_.data()) << std::endl;

        std::string response = "CLIENT " + std::to_string(session_id) + beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        auto const mut_buff = buffer_.prepare(response.size());
        memcpy(boost::asio::buffer_cast<void*>(mut_buff), response.data(), response.size());
        buffer_.commit(response.size());
        ws_.async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &Session::on_write,
                shared_from_this()));
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class WsStateServer : public std::enable_shared_from_this<WsStateServer> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    tcp::endpoint endpoint;
    uint8_t threads;
    uint32_t session_counter;

   public:
    WsStateServer(uint8_t threads, tcp::endpoint endpoint, net::io_context& ioc)
        : threads(threads),
          ioc_(ioc),
          acceptor_(ioc),
          endpoint(endpoint) {
    }

    void start_server() {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }

        do_accept();
    }

   private:
    void do_accept() {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&WsStateServer::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        std::cout << "New Connection : " << session_counter++ << std::endl;
        if (ec) {
            fail(ec, "accept");
        } else {
            // Create the session and run it
            std::make_shared<Session>(std::move(socket), session_counter)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

bool start_syncserver() {
    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = static_cast<unsigned short>(8000);
    auto const threads = 1;
    net::io_context ioc{threads};

    // Create and launch a listening port
    auto const server = std::make_shared<WsStateServer>(1, tcp::endpoint{address, port}, ioc);
    // std::make_shared<>(ioc, tcp::endpoint{address, port})->run();
    server->start_server();
    std::vector<std::thread> v;
    v.reserve(threads);
    for (auto i = 1; i <= threads; i++)
        v.emplace_back([&ioc, i] {
            std::cout << "Starting tcp thread " << i << std::endl;
            ioc.run();
        });

    sleep(10000000);
    return true;
}