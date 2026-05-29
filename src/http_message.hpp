#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct HttpRequest {
    std::string method;
    std::string uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    void clear() {
        method.clear();
        uri.clear();
        version.clear();
        headers.clear();
        body.clear();
    }
};

struct HttpResponse {
    std::string version = "HTTP/1.1";
    int status_code = 200;
    std::string status_message = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    std::string serialize() const {
        std::string result;
        result += version + " " + std::to_string(status_code) + " " + status_message + "\r\n";
        for (const auto& [key, value] : headers) {
            result += key + ": " + value + "\r\n";
        }
        // Only append Content-Length if it's not already in headers
        if (headers.find("Content-Length") == headers.end()) {
            result += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        }
        result += "\r\n";
        result.append(reinterpret_cast<const char*>(body.data()), body.size());
        return result;
    }
};
