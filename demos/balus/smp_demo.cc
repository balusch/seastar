/*
 * Copyright(C) 2022
 */

#include <chrono>

#include "seastar/core/abort_source.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/reactor.hh"

namespace ss = seastar;

extern ss::future<> f();

int main(int argc, char** argv) {
    ss::app_template app;
    return app.run(argc, argv, f);
}

ss::future<> f() {
    return ss::smp::invoke_on_all([]() {
        std::cout << &ss::engine() << std::endl;
        return ss::make_ready_future<>();
    });
}
