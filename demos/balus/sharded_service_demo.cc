
#include <chrono>

#include "seastar/core/abort_source.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/sharded.hh"

namespace ss = seastar;

[[maybe_unused]] static ss::future<> f();

int main(int argc, char** argv) {
    return 0;
}

static ss::future<> f() {
    return ss::make_ready_future<>();
}
