#include "server.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <cassert>

using namespace tokoro;

void worker_thread(uint16_t port, int num_requests, std::atomic<int>& successes, std::atomic<int>& failures) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        failures += num_requests;
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        ::close(sock);
        failures += num_requests;
        return;
    }

    std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    
    for (int i = 0; i < num_requests; ++i) {
        ssize_t sent = ::send(sock, req.c_str(), req.length(), 0);
        if (sent != static_cast<ssize_t>(req.length())) {
            failures++;
            continue;
        }

        std::string response;
        char buffer[4096];
        bool headers_done = false;
        while (!headers_done) {
            ssize_t bytes_read = ::recv(sock, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) break;
            response.append(buffer, bytes_read);
            if (response.find("\r\n\r\n") != std::string::npos) {
                headers_done = true;
            }
        }
        
        if (headers_done && response.find("200 OK") != std::string::npos) {
            successes++;
        } else {
            failures++;
        }
    }

    ::close(sock);
}

int main(int argc, char* argv[]) {
    int num_connections = 100;
    int requests_per_conn = 100;

    if (argc >= 3) {
        num_connections = std::stoi(argv[1]);
        requests_per_conn = std::stoi(argv[2]);
    }

    Config config;
    config.port = 0; // Bind to ephemeral port
    config.docroot = ".";
    config.workers = 4;
    config.header_timeout_ms = 5000;
    config.keepalive_timeout_ms = 5000;

    Server server(config);
    uint16_t bound_port = server.get_port();
    
    if (bound_port == 0) {
        std::cerr << "Failed to bind to port.\n";
        return 1;
    }

    std::cout << "Starting stress test on port " << bound_port << " with " 
              << num_connections << " connections, " 
              << requests_per_conn << " requests each...\n";

    std::atomic<bool> running{true};
    std::thread server_thread([&server, &running]() {
        server.run(running);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::atomic<int> successes{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_connections; ++i) {
        threads.emplace_back(worker_thread, bound_port, requests_per_conn, std::ref(successes), std::ref(failures));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = end - start;

    running.store(false);
    
    // Wake up server to shutdown
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(bound_port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    ::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    ::close(sock);
    
    server_thread.join();

    std::cout << "Test completed in " << diff.count() << " seconds.\n";
    std::cout << "Successes: " << successes.load() << "\n";
    std::cout << "Failures: " << failures.load() << "\n";

    if (failures > 0) {
        std::cerr << "Stress test failed with " << failures.load() << " failures.\n";
        return 1;
    }

    return 0;
}
