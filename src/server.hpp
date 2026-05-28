#ifndef TOKORO_SERVER_HPP
#define TOKORO_SERVER_HPP

#include "socket.hpp"
#include "thread_pool.hpp"
#include <cstdint>
#include <memory>

namespace tokoro {

class Server {
public:
    explicit Server(uint16_t port);

    // Starts the single-threaded accept loop
    void run();

private:
    void handle_client(std::shared_ptr<Socket> client_socket);

    uint16_t port_;
    Socket server_socket_;
    std::unique_ptr<ThreadPool> thread_pool_;
};

} // namespace tokoro

#endif // TOKORO_SERVER_HPP
