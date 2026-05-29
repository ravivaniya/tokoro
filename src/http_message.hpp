#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace tokoro {

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
    std::string file_path_to_send; // If set, body is empty and this file should be streamed

    std::string serialize_headers() const {
        std::string result;
        result += version + " " + std::to_string(status_code) + " " + status_message + "\r\n";
        for (const auto& [key, value] : headers) {
            result += key + ": " + value + "\r\n";
        }
        // Only append Content-Length if it's not already in headers
        if (headers.find("Content-Length") == headers.end()) {
            if (!file_path_to_send.empty()) {
                // If streaming, Content-Length should be set manually before calling this
            } else {
                result += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            }
        }
        result += "\r\n";
        return result;
    }

    std::string serialize() const {
        std::string result = serialize_headers();
        if (file_path_to_send.empty()) {
            result.append(reinterpret_cast<const char*>(body.data()), body.size());
        }
        return result;
    }
};

} // namespace tokoro
