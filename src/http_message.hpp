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
