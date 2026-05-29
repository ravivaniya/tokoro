#include "server.hpp"
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <stdexcept>
#include <sys/time.h>
#include <cerrno>
#include "http_parser.hpp"
#include "file_handler.hpp"

namespace tokoro {

Server::Server(uint16_t port) 
    : port_(port), 
      thread_pool_(std::make_unique<ThreadPool>(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4)) {
    if (!server_socket_.is_valid()) {
        throw std::runtime_error("Failed to create server socket.");
    }

    if (!server_socket_.set_reuse_address(true)) {
        throw std::runtime_error("Failed to set SO_REUSEADDR.");
    }

    if (!server_socket_.bind(port_)) {
        throw std::runtime_error("Failed to bind server socket to port " + std::to_string(port_));
    }

    if (!server_socket_.listen()) {
        throw std::runtime_error("Failed to listen on server socket.");
    }

    std::cout << "Server initialized and listening on port " << port_ << "...\n";
}

void Server::run() {
    if (!server_socket_.is_valid()) {
        std::cerr << "Server socket not valid, cannot run.\n";
        return;
    }

    while (true) {
        auto client_opt = server_socket_.accept();
        if (!client_opt) {
            std::cerr << "Failed to accept client connection.\n";
            continue;
        }

        std::cout << "Accepted new connection.\n";
        
        auto shared_client = std::make_shared<Socket>(std::move(*client_opt));
        thread_pool_->enqueue([this, shared_client]() {
            this->handle_client(shared_client);
        });
    }
}

void Server::handle_client(std::shared_ptr<Socket> client_socket) {
    // Set SO_RCVTIMEO
    struct timeval tv;
    tv.tv_sec = 5; // 5 seconds timeout
    tv.tv_usec = 0;
    if (setsockopt(client_socket->get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        std::cerr << "Error setting SO_RCVTIMEO\n";
    }

    std::vector<char> buffer(4096);
    HttpParser parser;
    HttpRequest req;

    bool keep_alive = true;

    while (keep_alive) {
        ssize_t bytes_received = ::recv(client_socket->get(), buffer.data(), buffer.size(), 0);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout
                std::cout << "Connection timed out.\n";
            } else {
                std::cerr << "Error receiving data.\n";
            }
            break;
        } else if (bytes_received == 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        std::string_view data_chunk(buffer.data(), bytes_received);
        
        while (!data_chunk.empty()) {
            // we could parse chunk by chunk, but parse() right now consumes all data
            // To handle pipelined requests properly, parse() should return how many bytes it consumed.
            // For now, we assume one request per recv(), or that parse will stop at Complete.
            // Wait, our parser stops and returns Complete, but doesn't tell us how much it consumed!
            // Actually, we pass the whole chunk. If there's extra data, it gets lost right now.
            // For this milestone, we'll just parse the chunk.

            auto result = parser.parse(data_chunk, req);

            if (result == ParseResult::Complete) {
                HttpResponse res = FileHandler::handle_request(req);
                std::string res_str = res.serialize();

                ssize_t bytes_sent = ::send(client_socket->get(), res_str.c_str(), res_str.size(), 0);
                if (bytes_sent < 0) {
                    std::cerr << "Error sending data.\n";
                    keep_alive = false;
                    break;
                }

                // Check keep-alive
                keep_alive = false;
                if (req.version == "HTTP/1.1") {
                    keep_alive = true;
                    auto it = req.headers.find("Connection");
                    if (it != req.headers.end() && it->second == "close") {
                        keep_alive = false;
                    }
                } else if (req.version == "HTTP/1.0") {
                    auto it = req.headers.find("Connection");
                    if (it != req.headers.end() && it->second == "keep-alive") {
                        keep_alive = true;
                    }
                }

                parser.reset();
                req.clear();
                break; // handled request, break out of data_chunk loop and wait for next recv
            } else if (result == ParseResult::Error) {
                std::cerr << "Parse error.\n";
                // Send 400 Bad Request
                HttpResponse res;
                res.status_code = 400;
                res.status_message = "Bad Request";
                std::string res_str = res.serialize();
                ::send(client_socket->get(), res_str.c_str(), res_str.size(), 0);
                keep_alive = false;
                break;
            } else {
                // Pending, need more data
                break; // break data_chunk loop, go to next recv
            }
        }
    }
}

} // namespace tokoro
