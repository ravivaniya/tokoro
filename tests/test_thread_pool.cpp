#include <catch2/catch_test_macros.hpp>
#include "thread_pool.hpp"
#include <atomic>
#include <chrono>

TEST_CASE("ThreadPool executes tasks correctly", "[thread_pool]") {
    std::atomic<int> counter{0};
    
    {
        tokoro::ThreadPool pool(4);
        for (int i = 0; i < 100; ++i) {
            pool.enqueue([&counter]() {
                counter++;
            });
        }
        // When pool goes out of scope, destructor will block until all tasks are finished
    }

    REQUIRE(counter.load() == 100);
}

TEST_CASE("ThreadPool handles varying number of threads", "[thread_pool]") {
    std::atomic<int> counter{0};
    
    {
        tokoro::ThreadPool pool(1); // Single thread
        for (int i = 0; i < 50; ++i) {
            pool.enqueue([&counter]() {
                counter++;
            });
        }
    }

    REQUIRE(counter.load() == 50);
}
