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
#include "logger.hpp"
#include "metrics.hpp"
#include "version.hpp"
#include <random>
#include <chrono>

namespace tokoro {

Server::Server(const Config& config) 
    : config_(config), 
      thread_pool_(std::make_unique<ThreadPool>(config_.workers)) {
    if (!server_socket_.is_valid()) {
        throw std::runtime_error("Failed to create server socket.");
    }

    Metrics::instance().set_queue_depth_callback([pool = thread_pool_.get()]() {
        return pool->queue_depth();
    });

    if (!server_socket_.set_reuse_address(true)) {
        throw std::runtime_error("Failed to set SO_REUSEADDR.");
    }

    if (!server_socket_.bind(config_.port)) {
        throw std::runtime_error("Failed to bind server socket to port " + std::to_string(config_.port));
    }

    if (!server_socket_.listen(128)) {
        throw std::runtime_error("Failed to listen on server socket.");
    }

    Logger::instance().info("Server initialized and listening on port " + std::to_string(config_.port) + "...");
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
    struct ActiveConnGuard {
        ActiveConnGuard() { Metrics::instance().inc_active_connections(); }
        ~ActiveConnGuard() { Metrics::instance().dec_active_connections(); }
    } conn_guard;

    std::string client_ip = client_socket.get_peer_ip();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;

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

        std::string_view data_chunk(buffer.data(), static_cast<size_t>(bytes_received));
        
        while (!data_chunk.empty()) {
            auto start_time = std::chrono::steady_clock::now();
            auto [result, bytes_consumed] = parser.parse(data_chunk, req);
            Metrics::instance().add_request_size_bytes(bytes_consumed);
            data_chunk = data_chunk.substr(bytes_consumed);

            if (result == ParseResult::Complete) {
                Metrics::instance().inc_requests_total();

                std::string req_id;
                auto rid_it = req.headers.find("X-Request-Id");
                if (rid_it != req.headers.end()) {
                    req_id = rid_it->second;
                } else {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "%08x%08x", dist(gen), dist(gen));
                    req_id = buf;
                }

                HttpResponse res = FileHandler::handle_request(req, config_.docroot);
                res.headers["X-Request-Id"] = req_id;
                
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
                while (static_cast<size_t>(total_sent) < res_str.size()) {
                    ssize_t bytes_sent = ::send(client_socket.get(), res_str.c_str() + total_sent, res_str.size() - static_cast<size_t>(total_sent), MSG_NOSIGNAL);
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
                                ssize_t sent = ::send(client_socket.get(), file_buf + chunk_sent, static_cast<size_t>(chunk_size - chunk_sent), MSG_NOSIGNAL);
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

                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                auto duration_sec = std::chrono::duration<double>(end_time - start_time).count();
                Metrics::instance().observe_request_duration(duration_sec);
                Metrics::instance().add_response_size_bytes(static_cast<uint64_t>(total_sent));
                
                std::string ua;
                auto ua_it = req.headers.find("User-Agent");
                if (ua_it != req.headers.end()) ua = ua_it->second;
                
                Logger::instance().access(req.method, req.uri, res.status_code, static_cast<size_t>(total_sent), static_cast<size_t>(duration_ms), client_ip, ua, req_id);

                parser.reset();
                req.clear();
                // Switch to keep-alive idle timeout
                set_timeout(config_.keepalive_timeout_ms);
                continue;
            } else if (result == ParseResult::Error) {
                Metrics::instance().inc_parse_errors_total();
                Logger::instance().error("Parse error on client " + client_ip);
                // Send 400 Bad Request
                HttpResponse res;
                res.status_code = 400;
                res.status_message = "Bad Request";
                res.headers["Connection"] = "close";
                res.headers["Server"] = std::string("tokoro/") + tokoro::VERSION;
                
                std::string res_str = res.serialize();
                ssize_t total_sent = 0;
                while (static_cast<size_t>(total_sent) < res_str.size()) {
                    ssize_t bytes_sent = ::send(client_socket.get(), res_str.c_str() + total_sent, res_str.size() - static_cast<size_t>(total_sent), MSG_NOSIGNAL);
                    if (bytes_sent < 0) break;
                    total_sent += bytes_sent;
                }
                Metrics::instance().add_response_size_bytes(static_cast<uint64_t>(total_sent));
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
