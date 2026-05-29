#pragma once

#include "http_message.hpp"

class FileHandler {
public:
    static HttpResponse handle_request(const HttpRequest& req);
};
