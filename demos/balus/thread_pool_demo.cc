
#include <memory>

#include "../../src/core/thread_pool.hh"
#include "seastar/core/app-template.hh"
#include "seastar/core/reactor.hh"

namespace ss = seastar;

int main(int argc, char **argv) {
    if (argc != 2) {
        fmt::fprintf(stderr, "usage: {} $file\n", argv[0]);
        exit(-1);
    }

    ss::app_template app;
    app.run(argc, argv, [f = argv[1]]() {
        // auto tp = std::make_unique<ss::thread_pool>(&ss::engine(), "laputa");

        return ss::engine()
            .file_stat(f, ss::follow_symlink::no)
            .then([f](ss::stat_data &&sd) {
                fmt::print(
                    "device_id:{}\tinode_number:{}\tmode:{}\ttype:{}\tnlinks:{}"
                    "\tuid:{}\tgid:{}\trdev:{}\tsize:{}\tblock_size:{}"
                    "\tallocated_size:{}\tatime:{}\tmtime:{}\tctime{}\n",
                    sd.device_id, sd.inode_number, sd.mode, sd.type,
                    sd.number_of_links, sd.uid, sd.gid, sd.rdev, sd.size,
                    sd.block_size, sd.allocated_size, sd.time_accessed,
                    sd.time_modified, sd.time_changed);
                return ss::make_ready_future<>();
            });
    });

    return 0;
}
