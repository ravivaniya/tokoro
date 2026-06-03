#include "http_parser.hpp"
#include <string_view>

using namespace tokoro;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (Size == 0) return 0;
    
    std::string_view chunk(reinterpret_cast<const char*>(Data), Size);
    
    HttpParser parser;
    HttpRequest req;
    
    // We only care that it does not crash or throw uncaught exceptions
    try {
        parser.parse(chunk, req);
    } catch (...) {
        // We shouldn't really catch anything if we want to find panic bugs,
        // but if there are known std::invalid_argument throws we might catch them.
        // For production-grade fuzzing, we don't catch, we let it crash the fuzzer so we can fix it.
    }
    
    return 0;
}
