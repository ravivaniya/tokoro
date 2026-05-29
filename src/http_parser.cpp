#include "http_parser.hpp"
#include <algorithm>
#include <cctype>

HttpParser::HttpParser() {
    reset();
}

void HttpParser::reset() {
    state_ = ParseState::RequestLineStart;
    current_header_name_.clear();
    current_header_value_.clear();
    expected_body_length_ = 0;
    body_bytes_read_ = 0;
    current_chunk_size_ = 0;
    is_chunked_ = false;
    current_request_line_bytes_ = 0;
    current_header_line_bytes_ = 0;
    headers_count_ = 0;
}

bool HttpParser::is_char(char c) const {
    return c >= 0 && c <= 127;
}

bool HttpParser::is_ctl(char c) const {
    return (c >= 0 && c <= 31) || c == 127;
}

bool HttpParser::is_tspecial(char c) const {
    switch (c) {
        case '(': case ')': case '<': case '>': case '@':
        case ',': case ';': case ':': case '\\': case '"':
        case '/': case '[': case ']': case '?': case '=':
        case '{': case '}': case ' ': case '\t':
            return true;
        default:
            return false;
    }
}

bool HttpParser::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

std::pair<ParseResult, size_t> HttpParser::parse(std::string_view data, HttpRequest& req) {
    if (state_ == ParseState::Error) {
        return {ParseResult::Error, 0};
    }
    if (state_ == ParseState::Complete) {
        return {ParseResult::Complete, 0};
    }

    size_t i = 0;
    for (; i < data.size(); ++i) {
        if (state_ >= ParseState::RequestLineStart && state_ <= ParseState::RequestLineVersion) {
            if (++current_request_line_bytes_ > max_request_line_bytes_) { state_ = ParseState::Error; return {ParseResult::Error, i + 1}; }
        } else if (state_ >= ParseState::HeaderLineStart && state_ <= ParseState::HeaderValue) {
            if (++current_header_line_bytes_ > max_header_bytes_) { state_ = ParseState::Error; return {ParseResult::Error, i + 1}; }
        }
        char c = data[i];

        switch (state_) {
            case ParseState::RequestLineStart:
                if (c == '\r' || c == '\n') {
                    // Ignore empty lines before request line as per RFC 7230 3.5
                    continue;
                } else if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                } else {
                    state_ = ParseState::RequestLineMethod;
                    req.method.push_back(c);
                }
                break;

            case ParseState::RequestLineMethod:
                if (c == ' ') {
                    state_ = ParseState::RequestLineUri;
                } else if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                } else {
                    req.method.push_back(c);
                }
                break;

            case ParseState::RequestLineUri:
                if (c == ' ') {
                    state_ = ParseState::RequestLineVersion;
                } else if (is_ctl(c)) {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                } else {
                    req.uri.push_back(c);
                }
                break;

            case ParseState::RequestLineVersion:
                if (c == '\r') {
                    // Expect \n next
                    continue;
                } else if (c == '\n') {
                    if (req.version.size() != 8 || req.version.substr(0, 5) != "HTTP/" ||
                        !is_digit(req.version[5]) || req.version[6] != '.' || !is_digit(req.version[7])) {
                        state_ = ParseState::Error;
                        return {ParseResult::Error, i + 1};
                    }
                    state_ = ParseState::HeaderLineStart;
                    current_header_line_bytes_ = 0;
                } else if (is_ctl(c)) {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                } else {
                    req.version.push_back(c);
                }
                break;

            case ParseState::HeaderLineStart:
                if (c == '\r') {
                    // Empty line means headers are done, transition to body
                    continue;
                } else if (c == '\n') {
                    // Headers done
                    // Check Content-Length or Transfer-Encoding
                    auto it_cl = req.headers.find("content-length");
                    if (it_cl != req.headers.end()) {
                        try {
                            expected_body_length_ = std::stoull(it_cl->second);
                            if (expected_body_length_ > max_body_bytes_) {
                                state_ = ParseState::Error;
                                return {ParseResult::Error, i + 1};
                            }
                        } catch (const std::exception&) {
                            state_ = ParseState::Error;
                            return {ParseResult::Error, i + 1};
                        }
                    }
                    auto it_te = req.headers.find("transfer-encoding");
                    if (it_te != req.headers.end()) {
                        if (it_te->second.find("chunked") != std::string::npos) {
                            is_chunked_ = true;
                        } else {
                            state_ = ParseState::Error;
                            return {ParseResult::Error, i + 1};
                        }
                    }

                    if (is_chunked_) {
                        state_ = ParseState::ChunkSize;
                        current_chunk_size_ = 0;
                    } else if (expected_body_length_ > 0) {
                        state_ = ParseState::Body;
                        req.body.reserve(expected_body_length_);
                    } else {
                        state_ = ParseState::Complete;
                        return {ParseResult::Complete, i + 1};
                    }
                } else if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                } else {
                    current_header_name_.push_back(c);
                    state_ = ParseState::HeaderName;
                }
                break;

            case ParseState::HeaderName:
                if (c == ':') {
                    state_ = ParseState::HeaderValue;
                } else if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                } else {
                    current_header_name_.push_back(c);
                }
                break;

            case ParseState::HeaderValue:
                if (c == '\r') {
                    // Expect \n next
                    continue;
                } else if (c == '\n') {
                    // Trim leading/trailing whitespace
                    size_t first = current_header_value_.find_first_not_of(" \t");
                    if (first != std::string::npos) {
                        size_t last = current_header_value_.find_last_not_of(" \t");
                        current_header_value_ = current_header_value_.substr(first, last - first + 1);
                    } else {
                        current_header_value_.clear();
                    }
                    
                    std::transform(current_header_name_.begin(), current_header_name_.end(), current_header_name_.begin(),
                        [](unsigned char ch){ return std::tolower(ch); });
                    req.headers[current_header_name_] = current_header_value_;
                    current_header_name_.clear();
                    current_header_value_.clear();
                    headers_count_++;
                    if (headers_count_ > max_headers_) {
                        state_ = ParseState::Error;
                        return {ParseResult::Error, i + 1};
                    }
                    state_ = ParseState::HeaderLineStart;
                    current_header_line_bytes_ = 0;
                } else {
                    current_header_value_.push_back(c);
                }
                break;

            case ParseState::Body:
                req.body.push_back(static_cast<uint8_t>(c));
                body_bytes_read_++;
                if (body_bytes_read_ >= expected_body_length_) {
                    state_ = ParseState::Complete;
                    return {ParseResult::Complete, i + 1};
                }
                break;

            case ParseState::ChunkSize:
                if (c == '\r') {
                    state_ = ParseState::ChunkSizeCRLF;
                } else if (c == ';') {
                    state_ = ParseState::ChunkExtension;
                } else {
                    int val = 0;
                    if (c >= '0' && c <= '9') val = c - '0';
                    else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
                    else {
                        state_ = ParseState::Error;
                        return {ParseResult::Error, i + 1};
                    }
                    current_chunk_size_ = (current_chunk_size_ * 16) + val;
                }
                break;

            case ParseState::ChunkExtension:
                if (c == '\r') {
                    state_ = ParseState::ChunkSizeCRLF;
                }
                break;

            case ParseState::ChunkSizeCRLF:
                if (c == '\n') {
                    if (current_chunk_size_ == 0) {
                        state_ = ParseState::ChunkTrailer;
                    } else {
                        state_ = ParseState::ChunkData;
                        body_bytes_read_ = 0;
                    }
                } else {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                }
                break;

            case ParseState::ChunkData:
                req.body.push_back(static_cast<uint8_t>(c));
                body_bytes_read_++;
                if (body_bytes_read_ >= current_chunk_size_) {
                    state_ = ParseState::ChunkDataCRLF;
                }
                break;

            case ParseState::ChunkDataCRLF:
                if (c == '\r') {
                    // skip
                } else if (c == '\n') {
                    state_ = ParseState::ChunkSize;
                    current_chunk_size_ = 0;
                } else {
                    state_ = ParseState::Error;
                    return {ParseResult::Error, i + 1};
                }
                break;

            case ParseState::ChunkTrailer:
                if (c == '\r') {
                    // skip
                } else if (c == '\n') {
                    state_ = ParseState::Complete;
                    return {ParseResult::Complete, i + 1};
                }
                // Simplified trailer parsing, ignore trailer contents until \r\n
                break;

            case ParseState::Complete:
            case ParseState::Error:
                break;
        }
    }

    return {(state_ == ParseState::Complete) ? ParseResult::Complete : (state_ == ParseState::Error ? ParseResult::Error : ParseResult::Pending), i};
}
