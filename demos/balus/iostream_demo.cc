
#include "seastar/core/app-template.hh"
#include "seastar/core/future.hh"
#include "seastar/core/sleep.hh"

namespace ss = seastar;

ss::future<> f1();

int main(int argc, char **argv) {
    ss::app_template app;
    return app.run(argc, argv, f1);
}

ss::future<> f1() {
    using namespace std::chrono_literals;
    return ss::sleep(1s).then([]() { fmt::print("sleep done...\n"); });
}
