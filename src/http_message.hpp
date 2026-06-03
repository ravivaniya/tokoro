#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <ctime>
#include "version.hpp"

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
    size_t file_offset = 0;
    size_t file_size_to_send = 0;
    bool omit_body = false;

    std::string serialize_headers() {
        if (headers.find("Date") == headers.end()) {
            char date_buf[128];
            time_t t = time(NULL);
            struct tm tm_info;
            gmtime_r(&t, &tm_info);
            strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm_info);
            headers["Date"] = std::string(date_buf);
        }
        if (headers.find("Server") == headers.end()) {
            headers["Server"] = std::string("tokoro/") + tokoro::VERSION;
        }

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

    std::string serialize() {
        std::string result = serialize_headers();
        if (!omit_body && file_path_to_send.empty()) {
            result.append(reinterpret_cast<const char*>(body.data()), body.size());
        }
        return result;
    }
};

} // namespace tokoro
