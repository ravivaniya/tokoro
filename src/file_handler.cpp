#include "file_handler.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <ctime>
#include "metrics.hpp"
#include "version.hpp"

namespace tokoro {

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

HttpResponse FileHandler::handle_request(const HttpRequest& req, const fs::path& docroot) {
    HttpResponse res;

    if (req.method == "OPTIONS") {
        res.status_code = 200;
        res.status_message = "OK";
        res.headers["Allow"] = "GET, HEAD, OPTIONS";
        res.headers["Content-Length"] = "0";
        res.omit_body = true;
        return res;
    }

    if (req.method != "GET" && req.method != "HEAD") {
        res.status_code = 405;
        res.status_message = "Method Not Allowed";
        res.headers["Allow"] = "GET, HEAD, OPTIONS";
        res.body = std::vector<uint8_t>{'4', '0', '5', ' ', 'M', 'e', 't', 'h', 'o', 'd', ' ', 'N', 'o', 't', ' ', 'A', 'l', 'l', 'o', 'w', 'e', 'd'};
        return res;
    }

    if (req.method == "HEAD") {
        res.omit_body = true;
    }

    if (req.uri == "/healthz" || req.uri == "/readyz") {
        res.status_code = 200;
        res.status_message = "OK";
        res.headers["Content-Type"] = "text/plain";
        std::string body = "OK";
        res.body = std::vector<uint8_t>(body.begin(), body.end());
        return res;
    }

    if (req.uri == "/metrics") {
        res.status_code = 200;
        res.status_message = "OK";
        res.headers["Content-Type"] = "text/plain";
        std::string metrics_str = Metrics::instance().to_prometheus_string();
        res.body = std::vector<uint8_t>(metrics_str.begin(), metrics_str.end());
        return res;
    }

    std::string decoded_path = url_decode(req.uri);
    
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
    
    fs::path requested_path;
    if (decoded_path.size() > 0 && decoded_path[0] == '/') {
        requested_path = fs::path(decoded_path.substr(1));
    } else {
        requested_path = fs::path(decoded_path);
    }
    fs::path target_path = fs::weakly_canonical(docroot / requested_path);
    
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

    auto file_size = fs::file_size(target_path);
    auto mtime = fs::last_write_time(target_path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(mtime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    time_t tt = std::chrono::system_clock::to_time_t(sctp);

    char date_buf[128];
    struct tm tm_info;
    gmtime_r(&tt, &tm_info);
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm_info);
    std::string last_modified(date_buf);

    std::string etag = "\"" + std::to_string(tt) + "-" + std::to_string(file_size) + "\"";

    res.headers["Last-Modified"] = last_modified;
    res.headers["ETag"] = etag;

    auto it_inm = req.headers.find("if-none-match");
    if (it_inm != req.headers.end() && it_inm->second == etag) {
        res.status_code = 304;
        res.status_message = "Not Modified";
        res.omit_body = true;
        return res;
    }

    auto it_ims = req.headers.find("if-modified-since");
    if (it_ims != req.headers.end() && it_ims->second == last_modified) {
        res.status_code = 304;
        res.status_message = "Not Modified";
        res.omit_body = true;
        return res;
    }

    size_t start = 0;
    size_t end = file_size > 0 ? file_size - 1 : 0;
    bool is_partial = false;
    auto it_range = req.headers.find("range");
    if (it_range != req.headers.end()) {
        std::string range_str = it_range->second;
        if (range_str.find("bytes=") == 0) {
            std::string byte_range = range_str.substr(6);
            size_t dash_pos = byte_range.find('-');
            if (dash_pos != std::string::npos) {
                std::string start_str = byte_range.substr(0, dash_pos);
                std::string end_str = byte_range.substr(dash_pos + 1);
                
                try {
                    if (start_str.empty() && !end_str.empty()) {
                        size_t suffix_len = std::stoull(end_str);
                        if (suffix_len > file_size) suffix_len = file_size;
                        start = file_size > 0 ? file_size - suffix_len : 0;
                        end = file_size > 0 ? file_size - 1 : 0;
                    } else if (!start_str.empty()) {
                        start = std::stoull(start_str);
                        if (!end_str.empty()) {
                            end = std::stoull(end_str);
                        }
                    }
                    
                    if (file_size > 0 && start >= file_size) {
                        res.status_code = 416;
                        res.status_message = "Range Not Satisfiable";
                        res.headers["Content-Range"] = "bytes */" + std::to_string(file_size);
                        res.omit_body = true;
                        return res;
                    } else if (file_size == 0) {
                        is_partial = false;
                    } else {
                        if (end >= file_size) end = file_size - 1;
                        is_partial = true;
                    }
                } catch (...) {
                    is_partial = false;
                }
            }
        }
    }

    if (is_partial) {
        res.status_code = 206;
        res.status_message = "Partial Content";
        res.headers["Content-Range"] = "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size);
        file_size = end - start + 1;
    } else {
        res.status_code = 200;
        res.status_message = "OK";
    }

    res.headers["Content-Length"] = std::to_string(file_size);

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

    if (!res.omit_body) {
        if (file_size > 1024 * 1024) { // Stream if larger than 1MB
            res.file_path_to_send = target_path.string();
            res.file_offset = start;
            res.file_size_to_send = file_size;
        } else {
            std::ifstream file(target_path, std::ios::binary);
            if (!file) {
                res.status_code = 500;
                res.status_message = "Internal Server Error";
                res.body = std::vector<uint8_t>{'5', '0', '0', ' ', 'I', 'n', 't', 'e', 'r', 'n', 'a', 'l', ' ', 'S', 'e', 'r', 'v', 'e', 'r', ' ', 'E', 'r', 'r', 'o', 'r'};
                return res;
            }
            if (is_partial && start > 0) {
                file.seekg(static_cast<std::streamoff>(start));
            }
            std::vector<char> buffer(file_size);
            if (file_size > 0) {
                file.read(buffer.data(), static_cast<std::streamsize>(file_size));
                res.body.assign(buffer.begin(), buffer.begin() + file.gcount());
            }
        }
    }

    return res;
}

} // namespace tokoro
