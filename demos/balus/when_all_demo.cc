
#include <chrono>
#include <vector>

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/sleep.hh"

namespace ss = seastar;

[[maybe_unused]] static ss::future<> f1();
[[maybe_unused]] static ss::future<> f2();

int main(int argc, char** argv) {
    for (auto& f : {f1, f2}) {
        ss::app_template app;
        app.run(argc, argv, f);
    }
    return 0;
}

static ss::future<> f1() {
    using namespace std::chrono_literals;
    return ss::sleep(1s).then([]() { fmt::print("fiber #1 sleep done...\n"); });
}

static ss::future<> f2() {
    using namespace std::chrono_literals;
    return ss::sleep(2s).then([]() { fmt::print("fiber #2 sleep done...\n"); });
}

#if 0
static ss::future<> f1() {
    using namespace std::chrono_literals;

    auto fut1 =
        ss::sleep(3s).then([]() { fmt::print("fiber #1 sleep done..."); });
    auto fut2 = ss::sleep(2s).then([]() {
        fmt::print("fiber #2 sleep done...\n");
        return ss::make_ready_future<int>(13);
    });
    auto func = []() {
        return ss::sleep(1s).then([]() {
            fmt::print("fiber #3 sleep done...\n");
            return ss::make_ready_future<double>(3.14);
        });
    };

    return ss::when_all(std::move(fut1), std::move(fut2), func)
        .then([](std::tuple<ss::future<>, ss::future<int>, ss::future<double>>&&
                     futs) {
            auto fut1 = std::move(std::get<0>(futs));
            if (fut1.failed()) {
                fmt::print("fiber #1 failed: {}\n", fut1.get_exception());
            }
        });
}

static ss::future<> f2() {
    using namespace std::chrono_literals;

    std::vector<ss::future<>> futs;

    futs.emplace_back(
        ss::sleep(2s).then([]() { fmt::print("fiber #1 sleep done...\n"); }));
    futs.emplace_back(
        ss::sleep(3s).then([]() { fmt::print("fiber #2 sleep done...\n"); }));
    futs.emplace_back(
        ss::sleep(1s).then([]() { fmt::print("fiber #3 sleep done...\n"); }));

    return ss::when_all(futs.begin(), futs.end())
        .then([](std::vector<ss::future<>>&& futs) {
            fmt::print("when_all() done...\n");
        });
}

#endif