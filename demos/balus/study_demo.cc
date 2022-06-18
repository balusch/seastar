
#include <boost/iterator/counting_iterator.hpp>
#include <chrono>
#include <iostream>

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/gate.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/thread.hh"

namespace ss = seastar;

using namespace std::chrono_literals;

extern ss::future<> f();

int main(int argc, char **argv) {
    ss::app_template app;
    return app.run(argc, argv, f);
}

template <typename T>
int do_sth(std::unique_ptr<T> obj) {
    std::cout << *obj << std::endl;
    return 13;
}

template <typename T>
ss::future<int> slow_do_sth(std::unique_ptr<T> obj) {
    return ss::sleep(1s).then(
        [obj = std::move(obj)]() mutable { return do_sth(std::move(obj)); });
}

ss::future<int> do_sth2() { return ss::make_ready_future<int>(13); }

class my_excption : public std::exception {
    virtual const char *what() const noexcept override {
        return "my exception";
    }
};

ss::future<> fail() { throw my_excption(); }

ss::future<> slow_op(const std::string &str) {
    return ss::sleep(100ms)
        .then([&str] { std::cout << "continuation 1: " << str << std::endl; })
        .then(
            [&str]() { std::cout << "continuation 2: " << str << std::endl; });
}

[[maybe_unused]] static ss::future<> test_fiber();
[[maybe_unused]] static ss::future<> test_gate();
[[maybe_unused]] static ss::future<int> my_test1();
[[maybe_unused]] static ss::future<> my_test2();
[[maybe_unused]] static ss::future<> my_test3();

ss::future<> f() {
    return my_test3();
}

static ss::future<> test_fiber() {
    /*
    ss::future<int> fut = ss::sleep(2s).then([] { return 3; });
    return ss::when_all(ss::sleep(1s), std::move(fut),
    ss::make_ready_future<double>(3.14)).discard_result();
    */

    /*
    ss::future<int> fut = ss::sleep(2s).then([] { return 3; });
    return ss::when_all(ss::sleep(1s), std::move(fut),
    ss::make_ready_future<double>(3.14)) .then([] (auto tup) { std::cout <<
    std::get<0>(tup).available() << std::endl; std::cout <<
    std::get<1>(tup).get0() << std::endl; std::cout << std::get<2>(tup).get0()
    << std::endl;
        });
    */

    ss::future<> slow_success = ss::sleep(2s);
    ss::future<> slow_exception = ss::sleep(1s).then([] { throw 1; });
    return ss::when_all(std::move(slow_success), std::move(slow_exception),
                        ss::make_ready_future<double>(3.14))
        .then([](auto tup) {
            std::cout << std::get<0>(tup).available() << std::endl;
            std::cout << std::get<1>(tup).failed() << std::endl;
            std::cout << std::get<2>(tup).get0() << std::endl;
            std::get<1>(tup).ignore_ready_future();
        });

    /*
    ss::future<int> fut = ss::sleep(2s).then([] { return 3; });
    return ss::when_all_succeed(ss::sleep(1s), std::move(fut),
    ss::make_ready_future<double>(3.14)) .then([] (auto tup) { std::cout <<
    std::get<0>(tup) << std::endl; std::cout << std::get<1>(tup) << std::endl;
        });
    */

    /*
    ss::future<int> fut = ss::sleep(2s).then([] { return 3; });
    return ss::when_all_succeed(ss::sleep(1s), std::move(fut),
    ss::make_exception_future<char>('h'), ss::make_ready_future<double>(1.3))
        .then([] (auto tup) {
            std::cout << std::get<0>(tup) << std::endl;
            std::cout << std::get<1>(tup) << std::endl;
    }).handle_exception([] (std::exception_ptr ep) {
        std::cout << "exception: " << ep << std::endl;
    });
    */
}

static ss::future<> slow2(int i, ss::gate &g) {
    auto print_now = []() {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };

    std::cerr << "starting " << i << "(" << print_now() << ")" << std::endl;
    return ss::do_for_each(boost::counting_iterator<int>(0),
                           boost::counting_iterator<int>(10),
                           [&g](int i) {
                               g.check();
                               return ss::sleep(1s);
                           })
        .finally([i, print_now] {
            std::cerr << "done" << i << "(" << print_now() << ")" << std::endl;
        });
}

static ss::future<> test_gate() {
    return ss::do_with(ss::gate(), [](ss::gate &g) {
        return ss::do_for_each(boost::counting_iterator<int>(1),
                               boost::counting_iterator<int>(6),
                               [&g](int i) {
                                   (void)ss::with_gate(
                                       g, [i, &g] { return slow2(i, g); });
                                   return ss::sleep(1s);
                               })
            .then([&g] {
                (void)ss::sleep(1s).then([&g] {
                    (void)ss::with_gate(g, [&g] { return slow2(6, g); });
                });
                return g.close();
            });
    });
}

static ss::future<int> my_test1() {
    return ss::sleep(1s).then([] { return 1; });
}

static ss::future<> my_test2() {
    auto fut = ss::sleep(1s);
    return fut.then([] {
        (void)ss::sleep(1s);
        return ss::make_ready_future<>();
    });
}

static ss::future<> my_test3() {
    return ss::smp::invoke_on_all([]() {
        return ss::sleep(1s).then([]() {
            std::cout << "Say hello on reactor-" << ss::this_shard_id()
                      << std::endl;
        });
    });
}