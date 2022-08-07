/*
 * Copyright(C) 2022
 */

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/thread.hh"
#include "seastar/http/httpd.hh"

namespace ss = seastar;

class stop_signal {
    bool _caught = false;
    ss::condition_variable _cond;

private:
    void signaled() {
        if (_caught) {
            return;
        }
        _caught = true;
        _cond.broadcast();
    }

public:
    stop_signal() {
        ss::engine().handle_signal(SIGINT, [this] { signaled(); });
        ss::engine().handle_signal(SIGTERM, [this] { signaled(); });
    }
    ~stop_signal() {
        // There's no way to unregister a handler yet, so register a no-op
        // handler instead.
        ss::engine().handle_signal(SIGINT, [] {});
        ss::engine().handle_signal(SIGTERM, [] {});
    }
    ss::future<> wait() {
        return _cond.wait([this] { return _caught; });
    }
    bool stopping() const { return _caught; }
};

[[maybe_unused]] static ss::future<> f1();
[[maybe_unused]] static ss::future<> f2();
[[maybe_unused]] static ss::future<> f3();

int main(int argc, char **argv) {
    ss::app_template app;
    return app.run(argc, argv, f1);
}

static ss::future<> f1() {
    return ss::async([]() {
        uint16_t port = 9877;
        stop_signal sg;
        auto server = new ss::httpd::http_server_control();
        server->start().get();
        server->listen(port).get();

        std::cout << "Seastar HTTP server listening on port " << port
                  << " ...\n";
        ss::engine().at_exit([server]() {
            std::cout << "Stoppping HTTP server" << std::endl;
            return server->stop();
        });
        sg.wait().get();
    });
}