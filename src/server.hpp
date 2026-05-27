#ifndef TOKORO_SERVER_HPP
#define TOKORO_SERVER_HPP

#include "socket.hpp"
#include <cstdint>

namespace tokoro {

class Server {
public:
    explicit Server(uint16_t port);

    // Starts the single-threaded accept loop
    void run();

private:
    void handle_client(Socket& client_socket);

    uint16_t port_;
    Socket server_socket_;
};

} // namespace tokoro

#endif // TOKORO_SERVER_HPP
