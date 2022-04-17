#include <memory>

#include "../../src/core/thread_pool.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/reactor.hh"

namespace ss = seastar;

[[maybe_unused]] static ss::future<> f1();
[[maybe_unused]] static ss::future<> f2();
[[maybe_unused]] static ss::future<> f3();

int main(int argc, char **argv) {
    ss::app_template app;
    app.run(argc, argv, []() { return f1(); });

    return 0;
}

static ss::future<> f1() {
    const char *f = "hello.txt";
    return ss::engine()
        .file_stat(f, ss::follow_symlink::no)
        .then([](ss::stat_data &&sd) {
            fmt::print(
                "device_id:{}\tinode_number:{}\tmode:{}\ttype:{}\tnlinks:{}"
                "\tuid:{}\tgid:{}\trdev:{}\tsize:{}\tblock_size:{}"
                "\tallocated_size:{}\tatime:{}\tmtime:{}\tctime{}\n",
                sd.device_id, sd.inode_number, sd.mode, (int)sd.type,
                sd.number_of_links, sd.uid, sd.gid, sd.rdev, sd.size,
                sd.block_size, sd.allocated_size,
                sd.time_accessed.time_since_epoch().count(),
                sd.time_modified.time_since_epoch().count(),
                sd.time_changed.time_since_epoch().count());
            return ss::make_ready_future<>();
        });
}

/* balus(N): bad example */
static ss::future<> f2() {
    auto tp = std::make_shared<ss::thread_pool>(&ss::engine(), "laputa");
    ss::engine().at_exit([tp]() {
        fmt::print("tp exit()\n");
        return ss::make_ready_future<>();
    });
    return tp
        ->submit<uint64_t>([tp]() {
            struct stat info;
            int rc = ::stat("hello.txt", &info);
            ss::throw_system_error_on(rc == -1, "stat() failed");
            return (uint64_t)info.st_size;
        })
        .then([](uint64_t &&size) { fmt::print("size: {}\n", size); });
}

static ss::future<> f3() {
    return ss::engine()
        .rename_file("hello.txt", "world.txt")
        .then_wrapped([](ss::future<> &&fut) {
            if (fut.failed()) {
                fmt::print("rename file failed: {}", fut.get_exception());
            }
            return ss::make_ready_future<>();
        });
}
