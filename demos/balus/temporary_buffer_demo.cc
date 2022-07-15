
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
    ss::temporary_buffer<char> buf1(1024);
    std::memcpy(buf1.get_write(), "Hello, Seastar!",
                sizeof("Hello, Seastar!") - 1);
    assert(buf1[0] == 'H');

    {
        auto buf2 = buf1.share();
        assert(buf2.size() == 1024);
        assert(buf2.begin() == buf1.begin() && buf2.end() == buf1.end());
    }

    {
        auto buf2 = buf1.clone();
        assert(buf2.size() == 1024);
        assert(buf2[0] == 'H');
        assert(buf2.begin() != buf1.begin() && buf2.end() != buf1.end());
    }

    {
        auto buf2 = buf1.share(1, 4);
        auto buf3 = buf2.clone();

        assert(buf3.size() == 4);
        assert(!(buf1.begin() <= buf3.begin() && buf3.end() <= buf1.end()));
    }

    {
        auto buf2 = ss::temporary_buffer<char>::aligned(16, 64);
        assert(buf2.size() == 64);
        assert(uintptr_t(buf2.begin()) % 16 == 0);
        fmt::print("buf1: {}-{}\tbuf2: {}-{}\n", fmt::ptr(buf1.begin()),
                   fmt::ptr(buf1.end()), fmt::ptr(buf2.begin()),
                   fmt::ptr(buf2.end()));
    }

    using namespace std::chrono_literals;
    return ss::sleep(1s).then([]() { fmt::print("sleep done...\n"); });
}