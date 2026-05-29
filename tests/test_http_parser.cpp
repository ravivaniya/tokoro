#include <catch2/catch_test_macros.hpp>
#include "http_parser.hpp"

TEST_CASE("HttpParser: Valid GET request", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test\r\n\r\n";
    
    auto result = parser.parse(data, req);
    
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.method == "GET");
    REQUIRE(req.uri == "/index.html");
    REQUIRE(req.version == "HTTP/1.1");
    REQUIRE(req.headers.size() == 2);
    REQUIRE(req.headers["Host"] == "localhost");
    REQUIRE(req.headers["User-Agent"] == "test");
    REQUIRE(req.body.empty());
}

TEST_CASE("HttpParser: Valid POST request with Content-Length", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST /api HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    
    auto result = parser.parse(data, req);
    
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.method == "POST");
    REQUIRE(req.uri == "/api");
    REQUIRE(req.version == "HTTP/1.1");
    REQUIRE(req.headers.size() == 1);
    REQUIRE(req.headers["Content-Length"] == "5");
    REQUIRE(req.body.size() == 5);
    std::string body_str(req.body.begin(), req.body.end());
    REQUIRE(body_str == "hello");
}

TEST_CASE("HttpParser: Incremental parsing (Pending state)", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view chunk1 = "GET / ";
    auto result1 = parser.parse(chunk1, req);
    REQUIRE(result1 == ParseResult::Pending);
    
    std::string_view chunk2 = "HTTP/1.1\r\nHost: a\r\n";
    auto result2 = parser.parse(chunk2, req);
    REQUIRE(result2 == ParseResult::Pending);
    
    std::string_view chunk3 = "\r\n";
    auto result3 = parser.parse(chunk3, req);
    REQUIRE(result3 == ParseResult::Complete);
    
    REQUIRE(req.method == "GET");
    REQUIRE(req.uri == "/");
    REQUIRE(req.version == "HTTP/1.1");
    REQUIRE(req.headers.size() == 1);
    REQUIRE(req.headers["Host"] == "a");
}

TEST_CASE("HttpParser: Malformed request line", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    // Invalid character in method
    std::string_view data = "GE\x01T / HTTP/1.1\r\n\r\n";
    auto result = parser.parse(data, req);
    REQUIRE(result == ParseResult::Error);
}

TEST_CASE("HttpParser: Valid chunked encoding", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\nF\r\n in \r\n\r\nchunks.\r\n0\r\n\r\n";
    auto result = parser.parse(data, req);
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.method == "POST");
    
    std::string body_str(req.body.begin(), req.body.end());
    REQUIRE(body_str == "Wikipedia in \r\n\r\nchunks.");
}

TEST_CASE("HttpParser: Valid chunked encoding with extensions", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4;ext=1\r\nWiki\r\n0\r\n\r\n";
    auto result = parser.parse(data, req);
    REQUIRE(result == ParseResult::Complete);
    
    std::string body_str(req.body.begin(), req.body.end());
    REQUIRE(body_str == "Wiki");
}
