#include <catch2/catch_test_macros.hpp>
#include "socket.hpp"
#include <sys/socket.h>

TEST_CASE("Socket RAII and move semantics", "[socket]") {
    tokoro::Socket sock;
    REQUIRE(sock.is_valid() == true);
    
    int fd = sock.get();
    
    // Move construct
    tokoro::Socket sock2(std::move(sock));
    REQUIRE(sock.is_valid() == false);
    REQUIRE(sock2.is_valid() == true);
    REQUIRE(sock2.get() == fd);
    
    // Move assign
    tokoro::Socket sock3;
    sock3 = std::move(sock2);
    
    REQUIRE(sock2.is_valid() == false);
    REQUIRE(sock3.is_valid() == true);
    REQUIRE(sock3.get() == fd);
    // fd3 should have been closed
}

TEST_CASE("Socket bind and listen", "[socket]") {
    tokoro::Socket server_sock;
    REQUIRE(server_sock.is_valid() == true);
    
    REQUIRE(server_sock.set_reuse_address(true) == true);
    
    // Use an ephemeral port or a testing port
    // We'll use 0 to let the OS pick a free port, which ensures the test never fails due to port conflict
    REQUIRE(server_sock.bind(0) == true);
    REQUIRE(server_sock.listen(10) == true);
}
