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

ParseResult HttpParser::parse(std::string_view data, HttpRequest& req) {
    if (state_ == ParseState::Error) {
        return ParseResult::Error;
    }
    if (state_ == ParseState::Complete) {
        return ParseResult::Complete;
    }

    for (size_t i = 0; i < data.size(); ++i) {
        char c = data[i];

        switch (state_) {
            case ParseState::RequestLineStart:
                if (c == '\r' || c == '\n') {
                    // Ignore empty lines before request line as per RFC 7230 3.5
                    continue;
                } else if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                    state_ = ParseState::Error;
                    return ParseResult::Error;
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
                    return ParseResult::Error;
                } else {
                    req.method.push_back(c);
                }
                break;

            case ParseState::RequestLineUri:
                if (c == ' ') {
                    state_ = ParseState::RequestLineVersion;
                } else if (is_ctl(c)) {
                    state_ = ParseState::Error;
                    return ParseResult::Error;
                } else {
                    req.uri.push_back(c);
                }
                break;

            case ParseState::RequestLineVersion:
                if (c == '\r') {
                    // Expect \n next
                    continue;
                } else if (c == '\n') {
                    state_ = ParseState::HeaderLineStart;
                } else if (is_ctl(c)) {
                    state_ = ParseState::Error;
                    return ParseResult::Error;
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
                    auto it_cl = req.headers.find("Content-Length");
                    if (it_cl != req.headers.end()) {
                        expected_body_length_ = std::stoull(it_cl->second);
                    }
                    auto it_te = req.headers.find("Transfer-Encoding");
                    if (it_te != req.headers.end()) {
                        if (it_te->second.find("chunked") != std::string::npos) {
                            // Chunked encoding not yet supported
                            state_ = ParseState::Error;
                            return ParseResult::Error;
                        }
                    }

                    if (expected_body_length_ > 0) {
                        state_ = ParseState::Body;
                        req.body.reserve(expected_body_length_);
                    } else {
                        state_ = ParseState::Complete;
                        return ParseResult::Complete;
                    }
                } else if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                    state_ = ParseState::Error;
                    return ParseResult::Error;
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
                    return ParseResult::Error;
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
                    
                    req.headers[current_header_name_] = current_header_value_;
                    current_header_name_.clear();
                    current_header_value_.clear();
                    state_ = ParseState::HeaderLineStart;
                } else {
                    current_header_value_.push_back(c);
                }
                break;

            case ParseState::Body:
                req.body.push_back(static_cast<uint8_t>(c));
                body_bytes_read_++;
                if (body_bytes_read_ >= expected_body_length_) {
                    state_ = ParseState::Complete;
                    // If there's more data in the view after body completes,
                    // in a real pipelined server we'd return and let caller consume the rest.
                    // For now, returning Complete is sufficient.
                    return ParseResult::Complete;
                }
                break;

            case ParseState::Complete:
            case ParseState::Error:
                break;
        }
    }

    return (state_ == ParseState::Complete) ? ParseResult::Complete : (state_ == ParseState::Error ? ParseResult::Error : ParseResult::Pending);
}
