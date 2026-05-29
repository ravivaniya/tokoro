#pragma once

#include "http_message.hpp"
#include <filesystem>

namespace tokoro {

class FileHandler {
public:
    static HttpResponse handle_request(const HttpRequest& req, const std::filesystem::path& docroot);
};

} // namespace tokoro
