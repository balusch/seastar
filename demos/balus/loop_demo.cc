
#include <array>
#include <chrono>
#include <iostream>

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/gate.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/smp.hh"
#include "seastar/core/thread.hh"

namespace ss = seastar;

extern ss::future<> f();
extern ss::future<> f2();
extern ss::future<> f3();
extern ss::future<> f4();

int main(int argc, char** argv) {
    ss::app_template app;
    return app.run(argc, argv, f4);
}

ss::future<> f() {
    auto task = []() {
        std::cout << "run on shard-" << ss::this_shard_id() << std::endl;
    };
    return ss::smp::invoke_on_others(task).then_wrapped([](ss::future<>&& fut) {
        if (fut.failed()) {
            std::cout << "invoke_on_others() failed: " << fut.get_exception()
                      << std::endl;
        } else {
            std::cout << "invoke_on_others() succeeded" << std::endl;
        }
    });
}

seastar::future<std::string> fetch(const std::string& domain,
                                   const std::string& object) {
    static int count = 0;
    return ++count != 2
             ? ss::make_exception_future<std::string>(
                   std::logic_error("logic error"))
             : ss::make_ready_future<std::string>("bbbbbbbbbbbbbbbbb");
}

#if 0
ss::future<std::string> search(const std::string &object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };
    for (size_t i = 0; i < domains.size(); i++) {
        auto fut = fetch(domains[i], object);
        if (!fut.failed()) {
            std::cout << fut.get0() << std::endl;
        }
    }
    return ss::make_ready_future<std::string>("");
}

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    int cur = 0;
    return ss::do_with(cur, [object = std::move(object)](int &cur) mutable {
        return ss::repeat([&cur, object = std::move(object)](){
            return fetch(domains[cur], object).then_wrapped([&cur](ss::future<std::string> &&fut) mutable {
                std::cout << "fetch from the " << cur << "-th domain: " << domains[cur];
                if (fut.failed()) {
                    std::cout << " FAILED: " << fut.get_exception() << std::endl;
                    cur = (cur + 1) % 3;
                    return ss::stop_iteration::no;
                }
                std::cout << " SUCCEEDED: " << fut.get0() << std::endl;
                return ss::stop_iteration::yes;
            });
        });
    }).then([]() { return ss::make_ready_future<std::string>("BBBBBBB"); });
}

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    int cur = 0;
    return ss::do_with(cur, [object = std::move(object)](int &cur) mutable {
        return ss::repeat([&cur, object = std::move(object)](){
            return fetch(domains[cur], object).then_wrapped([&cur](ss::future<std::string> &&fut) mutable {
                std::cout << "fetch from the " << cur << "-th domain: " << domains[cur];
                if (!fut.failed()) {
                    std::cout << " SUCCEEDED" << fut.get0() << std::endl;
                    return ss::stop_iteration::yes;
                }
                cur++;
                std::cout << " FAILED: " << fut.get_exception() << std::endl;
                return cur == domains.size() ? ss::stop_iteration::yes
                                             : ss::stop_iteration::no;
            });
        }).then([]() { return ss::make_ready_future<std::string>("BBBBBBB"); });
    });
}

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    int cur = 0;
    return ss::do_with(cur, [object = std::move(object)](int &cur) mutable {
        return ss::repeat_until_value([&cur, object = std::move(object)]() {
            return fetch(domains[cur], object).then_wrapped([&cur](ss::future<std::string> &&fut) mutable {
                std::cout << "fetch from the " << cur << "-th domain: " << domains[cur];
                if (!fut.failed()) {
                    std::cout << " SUCCEEDED" << std::endl;
                    return ss::make_ready_future<std::optional<std::string>>(fut.get0());
                }
                cur++;
                std::cout << " FAILED: " << fut.get_exception() << std::endl;
                return cur == domains.size() ? ss::make_exception_future<std::optional<std::string>>(std::runtime_error("all domain failed"))
                                             : ss::make_ready_future<std::optional<std::string>>(std::nullopt);
            });
        });
    });
};

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    return ss::do_for_each(domains, [object = std::move(object)](const std::string &domain) {
        return fetch(domain, object).then_wrapped([&domain](ss::future<std::string> &&fut) mutable {
            if (!fut.failed()) {
                std::cout << "fetch from domain: " << domain << " SUCCEEDED: " << fut.get0() << std::endl;
            } else {
                std::cout << "fetch from domain: " << domain << " FAILED: " << fut.get_exception() << std::endl;
            }
            return ss::make_ready_future<>();
        });
    }).then([]() { return ss::make_ready_future<std::string>("AAAAAAA"); });
}

static const char *response = "Seastar is the future";
ss::future<> f3() {
    ss::listen_options lo;
    lo.reuse_address = true;
    return ss::do_with(ss::listen(ss::make_ipv4_address({9877}), lo), [](ss::server_socket &listener) {
        return ss::keep_doing([&listener]() {
            return listener.accept().then_wrapped([](ss::future<ss::accept_result> &&fut) {
                if (fut.failed()) {
                    std::cerr << "accept() failed: " << fut.get_exception() << std::endl;
                } else {
                    auto ar = fut.get0();
                    std::cout << "accept() from " << ar.remote_address << std::endl;
                }
                return ss::make_ready_future<>();
            });
        });
    });
}

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    return ss::parallel_for_each(domains, [object = std::move(object)](std::string &domain) {
        return fetch(domain, object).then_wrapped([&domain](ss::future<std::string> &&fut) mutable {
            if (!fut.failed()) {
                std::cout << "fetch from domain: " << domain << " SUCCEEDED: " << fut.get0() << std::endl;
            } else {
                std::cout << "fetch from domain: " << domain << " FAILED: " << fut.get_exception() << std::endl;
            }
            return ss::make_ready_future<>();
        }).then([]() { using namespace std::chrono_literals; return ss::sleep(1s).then([]() { std::cout << "sleep done\n\n"; }); });
    }).then([]() { return ss::make_ready_future<std::string>("CCCCCCC") ;});
}

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    return ss::max_concurrent_for_each(domains, 2, [object = std::move(object)](std::string &domain) {
        return fetch(domain, object).then_wrapped([&domain](ss::future<std::string> &&fut) mutable {
            if (!fut.failed()) {
                std::cout << "fetch from domain: " << domain << " SUCCEEDED: " << fut.get0() << std::endl;
            } else {
                std::cout << "fetch from domain: " << domain << " FAILED: " << fut.get_exception() << std::endl;
            }
            return ss::make_ready_future<>();
        }).then([]() { using namespace std::chrono_literals; return ss::sleep(1s).then([]() { std::cout << "sleep done\n"; }); });
    }).then([]() { return ss::make_ready_future<std::string>("CCCCCCC") ;});
}

#elseif 0

ss::future<std::string> search(std::string &&object) {
    static std::vector<std::string> domains = {
            "www.google.com",
            "www.bing.com",
            "www.baidu.com",
    };

    return ss::do_with(int(), bool(), [object = std::move(object)](int &cur, bool &done)mutable {
        auto stop_when = [&done] { return done; };
        auto do_results = [&cur, &done, object = std::move(object)]() {
            return fetch(domains[cur], object).then_wrapped([&cur, &done](ss::future<std::string> &&fut) mutable {
                std::cout << "fetch from the " << cur << "-th domain: " << domains[cur];
                if (!fut.failed()) {
                    std::cout << " SUCCEEDED: " << fut.get0() << std::endl;
                    done = true;
                    return;
                }
                cur++;
                std::cout << " FAILED: " << fut.get_exception() << std::endl;
                if (cur == domains.size()) { done = true; }
            });
        };
        return ss::do_until(stop_when, do_results);
    }).then([]() { return ss::make_ready_future<std::string>(""); });
}

#else
ss::future<std::string> search(std::string&& object) {
    static std::vector<std::string> domains = {
        "www.google.com",
        "www.bing.com",
        "www.baidu.com",
    };

    auto result = ss::make_shared<std::string>();
    return ss::do_with(
               size_t(),
               [result, object = std::move(object)](size_t& cur) mutable {
                   return ss::repeat([&cur, result,
                                      object = std::move(object)]() {
                       return fetch(domains[cur], object)
                           .then_wrapped(
                               [&cur,
                                result](ss::future<std::string>&& fut) mutable {
                                   std::cout << "fetch from the " << cur
                                             << "-th domain: " << domains[cur];
                                   if (!fut.failed()) {
                                       *result = fut.get0();
                                       return ss::stop_iteration::yes;
                                   }
                                   cur++;
                                   std::cout
                                       << " FAILED: " << fut.get_exception()
                                       << std::endl;
                                   return cur == domains.size()
                                            ? ss::stop_iteration::yes
                                            : ss::stop_iteration::no;
                               });
                   });
               })
        .then([result]() {
            return ss::make_ready_future<std::string>(std::move(*result));
        });
}

#endif

ss::future<> f2() {
    return search("laputa").then_wrapped([](ss::future<std::string>&& fut) {
        if (fut.failed()) {
            std::cout << "search FAILED: " << fut.get_exception() << std::endl;
        } else {
            std::cout << "search SUCCEEDED: " << fut.get0() << std::endl;
        }
        return ss::make_ready_future<>();
    });
}

// TEST: do_for_each invoke action and failed in the middle of the iteration
ss::future<> f4() {
    auto arr = ss::make_shared<std::vector<int>>();
    *arr = {1, 3, 5, 6, 7, 9};
    auto counter = ss::make_shared<int>(0);
#if 1
    return ss::do_for_each(
               *arr,
#else
    return ss::parallel_for_each(
               *arr,
#endif
               [counter](int& ele) {
                   (*counter)++;
                   using namespace std::chrono_literals;
                   int nsleep = std::rand() % 6;
                   return ss::sleep(nsleep * 1s).then([ele, nsleep]() {
                       fmt::print("ele {} sleep for {} seconds\n", ele, nsleep);
                       if (ele % 2) {
                           return ss::make_ready_future<>();
                       } else {
                           return ss::make_exception_future<>(
                               std::runtime_error("do do do..."));
                       }
                   });
               })
        .then_wrapped([counter, arr](ss::future<>&& fut) {
            if (fut.failed()) {
                auto ex = fut.get_exception();
                fmt::print("do_for_each failed: {}\n", ex);
            } else {
                fmt::print("do_for_each succeeded\n");
            }
            fmt::print("counter: {}\n", *counter);
            return ss::make_ready_future<>();
        });
}