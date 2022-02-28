
#include <iostream>
#include <chrono>

#include "seastar/core/reactor.hh"
#include "seastar/core/smp.hh"
#include "seastar/core/gate.hh"
#include "seastar/core/thread.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/future.hh"
#include "seastar/core/future-util.hh"

namespace ss = seastar;

struct counter {
    explicit counter(int i) noexcept {
        c = i;
        std::cout << "construct counter #" << i << std::endl;
    }
    int c;
};

static ss::future<> f() __attribute__((unused));
static ss::future<> f1() __attribute__((unused));
static ss::future<> f2() __attribute__((unused));
static ss::future<> f3() __attribute__((unused));
static ss::future<> f4() __attribute__((unused));

int
main(int argc, char **argv)
{
    ss::app_template app;
    return app.run(argc, argv, f1);
}

// balus(n): Test evaluation order
static ss::future<> f() {
    using namespace std::chrono_literals;
    return ss::sleep(1s).then([c = counter(1)] () {
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
    return sem.wait(1).then([]() {
        std::cout << "semaphore wait OK: " << sem.available_units() << std::endl;
        return slow().finally([]() {
            sem.signal(1);
            std::cout << "slow finally: semaphore signal OK: " << sem.available_units() << std::endl;
        });
    }).finally([]() {
        std::cout << "wait finally: semaphore signal OK: " << sem.available_units() << std::endl;
    });
}