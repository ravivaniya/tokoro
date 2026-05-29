#include "file_handler.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::string url_decode(const std::string& src) {
    std::string ret;
    char ch;
    size_t i;
    int ii;
    for (i = 0; i < src.length(); i++) {
        if (src[i] == '%') {
            if (i + 2 < src.length()) {
                std::string hex = src.substr(i + 1, 2);
                try {
                    ii = std::stoi(hex, nullptr, 16);
                    ch = static_cast<char>(ii);
                    ret += ch;
                    i = i + 2;
                } catch (...) {
                    ret += src[i];
                }
            } else {
                ret += src[i];
            }
        } else if (src[i] == '+') {
            ret += ' ';
        } else {
            ret += src[i];
        }
    }
    return ret;
}

HttpResponse FileHandler::handle_request(const HttpRequest& req) {
    HttpResponse res;

    if (req.method != "GET") {
        res.status_code = 405;
        res.status_message = "Method Not Allowed";
        res.body = std::vector<uint8_t>{'4', '0', '5', ' ', 'M', 'e', 't', 'h', 'o', 'd', ' ', 'N', 'o', 't', ' ', 'A', 'l', 'l', 'o', 'w', 'e', 'd'};
        return res;
    }

    std::string decoded_path = url_decode(req.uri);
    
    // Reject NUL or control chars
    for (char c : decoded_path) {
        if ((c >= 0 && c <= 31) || c == 127) {
            res.status_code = 400;
            res.status_message = "Bad Request";
            res.body = std::vector<uint8_t>{'4', '0', '0', ' ', 'B', 'a', 'd', ' ', 'R', 'e', 'q', 'u', 'e', 's', 't'};
            return res;
        }
    }

    if (decoded_path == "/") {
        decoded_path = "/index.html";
    }
    
    fs::path docroot = fs::weakly_canonical(fs::absolute("./www"));
    fs::path requested_path;
    if (decoded_path.size() > 0 && decoded_path[0] == '/') {
        requested_path = fs::path(decoded_path.substr(1));
    } else {
        requested_path = fs::path(decoded_path);
    }
    fs::path target_path = fs::weakly_canonical(docroot / requested_path);
    
    // Check if the resolved target path starts with the docroot
    std::string target_str = target_path.string();
    std::string docroot_str = docroot.string();
    if (target_str.find(docroot_str) != 0) {
        res.status_code = 403;
        res.status_message = "Forbidden";
        res.body = std::vector<uint8_t>{'4', '0', '3', ' ', 'F', 'o', 'r', 'b', 'i', 'd', 'd', 'e', 'n'};
        return res;
    }
    
    if (!fs::exists(target_path) || !fs::is_regular_file(target_path)) {
        res.status_code = 404;
        res.status_message = "Not Found";
        res.body = std::vector<uint8_t>{'4', '0', '4', ' ', 'N', 'o', 't', ' ', 'F', 'o', 'u', 'n', 'd'};
        return res;
    }

    std::ifstream file(target_path, std::ios::binary);
    if (!file) {
        res.status_code = 500;
        res.status_message = "Internal Server Error";
        return res;
    }

    res.body = std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    res.status_code = 200;
    res.status_message = "OK";

    // Basic MIME type mapping
    std::string ext = target_path.extension().string();
    if (ext == ".html") {
        res.headers["Content-Type"] = "text/html";
    } else if (ext == ".css") {
        res.headers["Content-Type"] = "text/css";
    } else if (ext == ".js") {
        res.headers["Content-Type"] = "application/javascript";
    } else if (ext == ".png") {
        res.headers["Content-Type"] = "image/png";
    } else if (ext == ".jpg" || ext == ".jpeg") {
        res.headers["Content-Type"] = "image/jpeg";
    } else {
        res.headers["Content-Type"] = "application/octet-stream";
    }

    return res;
}
