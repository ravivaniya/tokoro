#include <catch2/catch_test_macros.hpp>
#include "http_parser.hpp"

TEST_CASE("HttpParser: Valid GET request", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test\r\n\r\n";
    
    auto [result, consumed] = parser.parse(data, req);
    
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.method == "GET");
    REQUIRE(req.uri == "/index.html");
    REQUIRE(req.version == "HTTP/1.1");
    REQUIRE(req.headers.size() == 2);
    REQUIRE(req.headers["host"] == "localhost");
    REQUIRE(req.headers["user-agent"] == "test");
    REQUIRE(req.body.empty());
}

TEST_CASE("HttpParser: Valid POST request with Content-Length", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST /api HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    
    auto [result, consumed] = parser.parse(data, req);
    
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.method == "POST");
    REQUIRE(req.uri == "/api");
    REQUIRE(req.version == "HTTP/1.1");
    REQUIRE(req.headers.size() == 1);
    REQUIRE(req.headers["content-length"] == "5");
    REQUIRE(req.body.size() == 5);
    std::string body_str(req.body.begin(), req.body.end());
    REQUIRE(body_str == "hello");
}

TEST_CASE("HttpParser: Incremental parsing (Pending state)", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view chunk1 = "GET / ";
    auto [result1, consumed1] = parser.parse(chunk1, req);
    REQUIRE(result1 == ParseResult::Pending);
    
    std::string_view chunk2 = "HTTP/1.1\r\nHost: a\r\n";
    auto [result2, consumed2] = parser.parse(chunk2, req);
    REQUIRE(result2 == ParseResult::Pending);
    
    std::string_view chunk3 = "\r\n";
    auto [result3, consumed3] = parser.parse(chunk3, req);
    REQUIRE(result3 == ParseResult::Complete);
    
    REQUIRE(req.method == "GET");
    REQUIRE(req.uri == "/");
    REQUIRE(req.version == "HTTP/1.1");
    REQUIRE(req.headers.size() == 1);
    REQUIRE(req.headers["host"] == "a");
}

TEST_CASE("HttpParser: Malformed request line", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    // Invalid character in method
    std::string_view data = "GE\x01T / HTTP/1.1\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Error);
}

TEST_CASE("HttpParser: Valid chunked encoding", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\nF\r\n in \r\n\r\nchunks.\r\n0\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.method == "POST");
    
    std::string body_str(req.body.begin(), req.body.end());
    REQUIRE(body_str == "Wikipedia in \r\n\r\nchunks.");
}

TEST_CASE("HttpParser: Valid chunked encoding with extensions", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4;ext=1\r\nWiki\r\n0\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Complete);
    
    std::string body_str(req.body.begin(), req.body.end());
    REQUIRE(body_str == "Wiki");
}

TEST_CASE("HttpParser: Case-insensitive headers", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "GET / HTTP/1.1\r\nHoSt: localhost\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.headers.find("host") != req.headers.end());
    REQUIRE(req.headers["host"] == "localhost");
}

TEST_CASE("HttpParser: Pipelined requests", "[parser]") {
    HttpParser parser;
    HttpRequest req1;
    
    std::string_view data = "GET /1 HTTP/1.1\r\nHost: a\r\n\r\nGET /2 HTTP/1.1\r\nHost: b\r\n\r\n";
    auto [result1, consumed1] = parser.parse(data, req1);
    REQUIRE(result1 == ParseResult::Complete);
    REQUIRE(req1.uri == "/1");
    REQUIRE(consumed1 == 28); // length of first request
    
    data = data.substr(consumed1);
    parser.reset();
    HttpRequest req2;
    auto [result2, consumed2] = parser.parse(data, req2);
    REQUIRE(result2 == ParseResult::Complete);
    REQUIRE(req2.uri == "/2");
}

TEST_CASE("HttpParser: Oversized header line", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string data = "GET / HTTP/1.1\r\n";
    data += "X-Large: ";
    data.append(20000, 'a');
    data += "\r\n\r\n";
    
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Error);
}

TEST_CASE("HttpParser: Invalid Content-Length", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Error);
}

TEST_CASE("HttpParser: Oversized Content-Length", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "POST / HTTP/1.1\r\nContent-Length: 20000000\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Error);
}

TEST_CASE("HttpParser: Tab in header value", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "GET / HTTP/1.1\r\nCustom-Header: value\twith\ttab\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Complete);
    REQUIRE(req.headers["custom-header"] == "value\twith\ttab");
}

TEST_CASE("HttpParser: Invalid HTTP version string", "[parser]") {
    HttpParser parser;
    HttpRequest req;
    
    std::string_view data = "GET / HTTP/1.14\r\n\r\n";
    auto [result, consumed] = parser.parse(data, req);
    REQUIRE(result == ParseResult::Error);
}
