#include <catch2/catch_test_macros.hpp>
#include "server.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <vector>

using namespace tokoro;

std::string send_request(uint16_t port, const std::string& request_data) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(sock != -1);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(sock);
        return "";
    }

    ::send(sock, request_data.c_str(), request_data.length(), 0);

    std::string response;
    char buffer[4096];
    while (true) {
        ssize_t bytes_read = ::recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) break;
        response.append(buffer, bytes_read);
        // Basic check for end of headers + content-length or connection close
        if (response.find("\r\n\r\n") != std::string::npos) {
            // For these simple tests, we assume connection close or no body
            // If body is present, we might need to read more, but we just wait until close for simplicity
        }
    }

    ::close(sock);
    return response;
}

TEST_CASE("Server Integration Tests", "[integration]") {
    Config config;
    config.port = 0; // Bind to ephemeral port
    config.docroot = ".";
    config.workers = 2;
    config.header_timeout_ms = 1000;
    config.keepalive_timeout_ms = 1000;

    Server server(config);
    uint16_t bound_port = server.get_port();
    REQUIRE(bound_port > 0);

    std::atomic<bool> running{true};
    std::thread server_thread([&server, &running]() {
        server.run(running);
    });

    // Give the server a moment to start the accept loop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SECTION("Happy Path GET Request") {
        std::string req = "GET / HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        std::string resp = send_request(bound_port, req);
        
        REQUIRE(resp.find("HTTP/1.0 200 OK") != std::string::npos);
        // The server currently sends HTTP/1.0 response for HTTP/1.0 request because of our fixes? 
        // Actually, file_handler defaults to HTTP/1.1 if not careful, but let's just check 200 OK
        REQUIRE(resp.find("200 OK") != std::string::npos);
    }

    SECTION("Unsupported HTTP Version") {
        std::string req = "GET / HTTP/0.9\r\nHost: localhost\r\n\r\n";
        std::string resp = send_request(bound_port, req);
        
        REQUIRE(resp.find("505") != std::string::npos);
    }

    SECTION("Bad Request - Malformed Header") {
        std::string req = "GET / HTTP/1.1\r\nMalformed Header\r\n\r\n";
        std::string resp = send_request(bound_port, req);
        
        REQUIRE(resp.find("400") != std::string::npos);
    }
    
    SECTION("Slowloris basic defense") {
        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock != -1);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(bound_port);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        REQUIRE(::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == 0);

        // Send one byte
        ::send(sock, "G", 1, 0);
        
        // Wait longer than header_timeout_ms (1000ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        
        char buf[1];
        ssize_t res = ::recv(sock, buf, 1, 0);
        // The server should have closed the connection
        REQUIRE(res <= 0);
        
        ::close(sock);
    }

    running.store(false);
    // Send a dummy request to wake up poll() in the server
    send_request(bound_port, "GET / HTTP/1.0\r\n\r\n");
    server_thread.join();
}
