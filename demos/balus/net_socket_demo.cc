
#include "seastar/core/app-template.hh"
#include "seastar/core/future.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/sleep.hh"
#include "seastar/net/api.hh"

namespace ss = seastar;

[[maybe_unused]] static ss::future<> f1();

int main(int argc, char **argv) {
    ss::app_template app;
    return app.run(argc, argv, f1);
}

static ss::future<> f1() {
    ss::listen_options lopt;
    ss::socket_address sa(uint16_t(9877));
    ss::server_socket s = ss::engine().listen(sa, lopt);
    return s.accept().then([](ss::accept_result ar) {
        auto conn = std::move(ar.connection);
        auto peer = std::move(ar.remote_address);
        return ss::make_ready_future<>();
    });
}
