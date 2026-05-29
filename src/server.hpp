#ifndef TOKORO_SERVER_HPP
#define TOKORO_SERVER_HPP

#include "socket.hpp"
#include "thread_pool.hpp"
#include "config.hpp"
#include <cstdint>
#include <memory>
#include <atomic>

namespace tokoro {

class Server {
public:
    explicit Server(const Config& config);

    // Starts the single-threaded accept loop
    void run(std::atomic<bool>& running);

private:
    void handle_client(Socket client_socket);

    Config config_;
    Socket server_socket_;
    std::unique_ptr<ThreadPool> thread_pool_;
};

} // namespace tokoro

#endif // TOKORO_SERVER_HPP
