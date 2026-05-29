#include "server.hpp"
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <stdexcept>
#include <sys/time.h>
#include <cerrno>
#include <poll.h>
#include <fstream>
#include "http_parser.hpp"
#include "file_handler.hpp"

namespace tokoro {

Server::Server(const Config& config) 
    : config_(config), 
      thread_pool_(std::make_unique<ThreadPool>(config_.workers)) {
    if (!server_socket_.is_valid()) {
        throw std::runtime_error("Failed to create server socket.");
    }

    if (!server_socket_.set_reuse_address(true)) {
        throw std::runtime_error("Failed to set SO_REUSEADDR.");
    }

    if (!server_socket_.bind(config_.port)) {
        throw std::runtime_error("Failed to bind server socket to port " + std::to_string(config_.port));
    }

    if (!server_socket_.listen(128)) {
        throw std::runtime_error("Failed to listen on server socket.");
    }

    std::cout << "Server initialized and listening on port " << config_.port << "...\n";
}

void Server::run(std::atomic<bool>& running) {
    if (!server_socket_.is_valid()) {
        std::cerr << "Server socket not valid, cannot run.\n";
        return;
    }

    while (running.load(std::memory_order_relaxed)) {
        struct pollfd pfd;
        pfd.fd = server_socket_.get();
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 500); // 500ms timeout
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::cerr << "poll error on listen socket\n";
            break;
        }

        if (ret == 0) {
            // timeout, check running flag again
            continue;
        }

        if (pfd.revents & POLLIN) {
            auto client_opt = server_socket_.accept();
            if (!client_opt) {
                std::cerr << "Failed to accept client connection.\n";
                continue;
            }

            auto shared_client = std::make_shared<Socket>(std::move(*client_opt));
            thread_pool_->enqueue([this, shared_client]() {
                this->handle_client(std::move(*shared_client));
            });
        }
    }
    std::cout << "Server shutting down...\n";
}

void Server::handle_client(Socket client_socket) {
    auto set_timeout = [&](size_t ms) {
        struct timeval tv;
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        ::setsockopt(client_socket.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    };

    set_timeout(config_.header_timeout_ms);

    std::vector<char> buffer(4096);
    HttpParser parser;
    HttpRequest req;

    bool keep_alive = true;

    while (keep_alive) {
        ssize_t bytes_received = ::recv(client_socket.get(), buffer.data(), buffer.size(), 0);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout
            } else {
                std::cerr << "Error receiving data.\n";
            }
            break;
        } else if (bytes_received == 0) {
            break;
        }

        std::string_view data_chunk(buffer.data(), bytes_received);
        
        while (!data_chunk.empty()) {
            auto [result, bytes_consumed] = parser.parse(data_chunk, req);
            data_chunk = data_chunk.substr(bytes_consumed);

            if (result == ParseResult::Complete) {
                HttpResponse res = FileHandler::handle_request(req, config_.docroot);
                
                // Determine keep-alive
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

                if (keep_alive) {
                    res.headers["Connection"] = "keep-alive";
                } else {
                    res.headers["Connection"] = "close";
                }

                std::string res_str = res.serialize();

                ssize_t total_sent = 0;
                while (total_sent < res_str.size()) {
                    ssize_t bytes_sent = ::send(client_socket.get(), res_str.c_str() + total_sent, res_str.size() - total_sent, MSG_NOSIGNAL);
                    if (bytes_sent < 0) {
                        std::cerr << "Error sending data.\n";
                        keep_alive = false;
                        break;
                    }
                    total_sent += bytes_sent;
                }

                if (keep_alive && !res.file_path_to_send.empty()) {
                    std::ifstream file(res.file_path_to_send, std::ios::binary);
                    if (file) {
                        char file_buf[65536];
                        while (file.read(file_buf, sizeof(file_buf)) || file.gcount() > 0) {
                            ssize_t chunk_sent = 0;
                            ssize_t chunk_size = file.gcount();
                            while (chunk_sent < chunk_size) {
                                ssize_t sent = ::send(client_socket.get(), file_buf + chunk_sent, chunk_size - chunk_sent, MSG_NOSIGNAL);
                                if (sent < 0) {
                                    keep_alive = false;
                                    break;
                                }
                                chunk_sent += sent;
                            }
                            if (!keep_alive) break;
                        }
                    } else {
                        keep_alive = false;
                    }
                }
                
                if (!keep_alive) break;

                parser.reset();
                req.clear();
                // Switch to keep-alive idle timeout
                set_timeout(config_.keepalive_timeout_ms);
                continue;
            } else if (result == ParseResult::Error) {
                std::cerr << "Parse error.\n";
                // Send 400 Bad Request
                HttpResponse res;
                res.status_code = 400;
                res.status_message = "Bad Request";
                res.headers["Connection"] = "close";
                res.headers["Server"] = "tokoro/1.0";
                
                std::string res_str = res.serialize();
                ssize_t total_sent = 0;
                while (total_sent < res_str.size()) {
                    ssize_t bytes_sent = ::send(client_socket.get(), res_str.c_str() + total_sent, res_str.size() - total_sent, MSG_NOSIGNAL);
                    if (bytes_sent < 0) break;
                    total_sent += bytes_sent;
                }
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
