#pragma once

#include "http_message.hpp"
#include <string_view>
#include <cstddef>

enum class ParseState {
    RequestLineStart,
    RequestLineMethod,
    RequestLineUri,
    RequestLineVersion,
    HeaderLineStart,
    HeaderName,
    HeaderValue,
    Body,
    ChunkSize,
    ChunkExtension,
    ChunkSizeCRLF,
    ChunkData,
    ChunkDataCRLF,
    ChunkTrailer,
    Complete,
    Error
};

enum class ParseResult {
    Pending,
    Complete,
    Error
};

class HttpParser {
public:
    HttpParser();

    // Parses a chunk of data. Updates `req` as it parses.
    // Returns ParseResult indicating the overall state of the parsing process.
    ParseResult parse(std::string_view data, HttpRequest& req);

    void reset();

private:
    ParseState state_;
    std::string current_header_name_;
    std::string current_header_value_;
    size_t expected_body_length_;
    size_t body_bytes_read_;
    size_t current_chunk_size_;
    bool is_chunked_;

    // Helper functions for parsing
    bool is_char(char c) const;
    bool is_ctl(char c) const;
    bool is_tspecial(char c) const;
    bool is_digit(char c) const;
};
