
#include <seastar/core/app-template.hh>
#include <seastar/core/file.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/future.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/sleep.hh>

namespace ss = seastar;

ss::future<> f1();

int main(int argc, char **argv) {
    ss::app_template app;
    return app.run(argc, argv, f1);
}

ss::future<> f1() {
    static char small_buf[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static char large_buf[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    return ss::open_file_dma("hello.txt", ss::open_flags::wo |
                                              ss::open_flags::create |
                                              ss::open_flags::exclusive)
        .then([](ss::file &&f) {
            return ss::make_file_output_stream(f, 32).then(
                [](ss::output_stream<char> &&os) mutable {
                    return os.write(small_buf, sizeof(small_buf))
                        .then([os = std::move(os)]() mutable {
                            return os.write(large_buf, sizeof(large_buf));
                        });
                });
        });
}
