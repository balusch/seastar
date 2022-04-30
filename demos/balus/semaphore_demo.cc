
#include <chrono>
#include <iostream>
#include <random>

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/thread.hh"

namespace ss = seastar;

struct counter {
    explicit counter(int i) noexcept {
        c = i;
        std::cout << "construct counter #" << i << std::endl;
    }
    int c;
};

[[maybe_unused]] static ss::future<> f();
[[maybe_unused]] static ss::future<> f1();
[[maybe_unused]] static ss::future<> f2();

int main(int argc, char** argv) {
    ss::app_template app;
    return app.run(argc, argv, f2);
}

// balus(n): Test evaluation order
static ss::future<> f() {
    using namespace std::chrono_literals;
    return ss::sleep(1s).then([c = counter(1)]() {
        std::cout << "first sleep done" << std::endl;
        return ss::sleep(1s).then([c = counter(2)]() {
            std::cout << "second sleep done" << std::endl;
        });
    });
}

static ss::future<> slow() {
    throw std::logic_error("LOGIC ERROR");
    using namespace std::chrono_literals;
    return ss::sleep(1s);
}

// balus(n): Test semaphore wait/signal
static ss::future<> f1() {
    static ss::semaphore sem(10);
    return sem.wait(1)
        .then([]() {
            std::cout << "semaphore wait OK: " << sem.available_units()
                      << std::endl;
            return slow().finally([]() {
                sem.signal(1);
                std::cout << "slow finally: semaphore signal OK: "
                          << sem.available_units() << std::endl;
            });
        })
        .finally([]() {
            std::cout << "wait finally: semaphore signal OK: "
                      << sem.available_units() << std::endl;
        });
}

// balus(T): test fiber interleave and with_semaphore
static int data = 0;
static ss::semaphore sema{1};

static ss::future<> f2() {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    static std::uniform_real_distribution<double> dist(0.0, 4.0);

    using namespace std::chrono_literals;
    [[maybe_unused]] auto modify_data1 = [](int d) {
        std::chrono::duration dur = static_cast<int>(dist(mt)) * 1s;
        (void)ss::sleep(dur).then([d, dur]() {
            fmt::print("set data to {} after sleep {} seconds\n", d,
                       dur.count());
            data = d;
        });
    };

    [[maybe_unused]] auto modify_data2 = [](int d) {
        (void)ss::with_semaphore(sema, 1, [d]() {
            std::chrono::duration dur = static_cast<int>(dist(mt)) * 1s;
            return ss::sleep(dur).then([d, dur]() {
                fmt::print("set data to {} after sleep {} seconds\n", d,
                           dur.count());
                data = d;
            });
        });
    };

#if 0

    // the final result is undetermined
    modify_data1(3);
    modify_data1(7);

#else

    // the final result is always 7
    modify_data2(3);
    modify_data2(7);

#endif

    return ss::sleep(5s).then(
        []() { fmt::print("finally data is {}\n", data); });
}