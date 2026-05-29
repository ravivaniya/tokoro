
#include <catch2/catch_test_macros.hpp>
#include "file_handler.hpp"
#include "http_message.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("FileHandler: Path Traversal Prevention", "[file_handler]") {
    fs::create_directories("./www");
    std::ofstream("./www/index.html") << "hello";

    HttpRequest req;
    req.method = "GET";
    
    // Directory traversal literal
    req.uri = "/../../CMakeLists.txt";
    auto res1 = FileHandler::handle_request(req);
    REQUIRE(res1.status_code == 403);
    
    // URL-encoded traversal
    req.uri = "/%2e%2e/%2e%2e/CMakeLists.txt";
    auto res2 = FileHandler::handle_request(req);
    REQUIRE(res2.status_code == 403);
    
    // NUL byte injection
    req.uri = "/index.html%00";
    auto res3 = FileHandler::handle_request(req);
    REQUIRE(res3.status_code == 400);

    // Absolute path
    req.uri = "//etc/passwd";
    auto res4 = FileHandler::handle_request(req);
    REQUIRE(res4.status_code == 403);
}
