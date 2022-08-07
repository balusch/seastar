
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

class simple_server {
public:
    simple_server() = default;
    ~simple_server() = default;

    ss::future<> accept();

private:
};

[[maybe_unused]] static ss::future<> process_connection(
    ss::connected_socket cs);

static ss::future<> f1() {
    ss::listen_options lopt;
    ss::socket_address sa(uint16_t(9877));
    ss::server_socket s = ss::listen(sa, lopt);
    return ss::repeat([s = std::move(s)]() mutable {
        return s.accept().then([](ss::accept_result ar) {
            std::cout << "accept connection from: " << ar.remote_address
                      << std::endl;
            return ss::stop_iteration();
        });
    });
}
