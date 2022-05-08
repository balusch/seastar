
#include <chrono>

#include "seastar/core/app-template.hh"
#include "seastar/core/do_with.hh"
#include "seastar/core/future.hh"
#include "seastar/core/reactor.hh"
#include "seastar/core/sleep.hh"
#include "seastar/core/thread.hh"

namespace ss = seastar;

[[maybe_unused]] static ss::future<> f1();
[[maybe_unused]] static ss::future<> f2();
[[maybe_unused]] static ss::future<> f3();

int main(int argc, char **argv) {
    ss::app_template app;
    return app.run(argc, argv, f3);
}

static ss::future<> f1() {
    ss::thread th([]() {
        std::cout << "Hi, seastar thread!" << std::endl;
        for (int i = 0; i < 4; i++) {
            using namespace std::chrono_literals;
            ss::sleep(2s).wait();
            std::cout << i << "-th wakeup, now: "
                      << std::chrono::steady_clock::now().time_since_epoch() /
                             std::chrono::seconds(1)
                      << std::endl;
        }
    });
    return ss::do_with(std::move(th), [](seastar::thread &th) {
        return th.join().then_wrapped([](ss::future<> &&fut) {
            if (fut.failed()) {
                auto ex = fut.get_exception();
                std::cout << "join thread failed: " << ex << std::endl;
                return ss::make_exception_future<>(std::move(ex));
            }

            std::cout << "join thread succeeded" << std::endl;
            return ss::make_ready_future<>();
        });
    });
}

static ss::future<> f2() {
    return ss::async([]() {
        std::cout << "Hi, seastar thread!" << std::endl;
        for (int i = 0; i < 4; i++) {
            using namespace std::chrono_literals;
            ss::sleep(2s).wait();
            std::cout << i << "-th wakeup, now: "
                      << std::chrono::steady_clock::now().time_since_epoch() /
                             std::chrono::seconds(1)
                      << std::endl;
        }
    });
}

static ss::future<> f3() {
    ss::sstring filename = "hello.txt";
    return ss::async([filename]() {
               ss::file f =
                   ss::open_file_dma(filename, ss::open_flags::ro).get0();
               ss::temporary_buffer<char> buf = f.dma_read<char>(0, 64).get0();
               return ss::sstring(buf.get(), buf.size());
           })
        .then([](ss::sstring &&buf) { std::cout << buf << std::endl; });
}