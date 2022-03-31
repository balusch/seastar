
#include <vector>

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"
#include "seastar/core/rwlock.hh"
#include "seastar/core/sleep.hh"

namespace ss = seastar;

static ss::future<> f();

int main(int argc, char** argv) {
    ss::app_template app;
    app.run(argc, argv, f);
    return 0;
}

static ss::future<> f() {
    using namespace std::chrono_literals;

    ss::rwlock lock;
    std::vector<ss::future<>> futs;

    auto fut = lock.read_lock()
                   .then([]() {
                       fmt::print("fiber #1 get lock...\n");
                       return ss::sleep(4s);
                   })
                   .then([]() {
                       fmt::print("fiber #1 sleep done...\n");
                       return ss::make_ready_future<>();
                   })
                   .finally([&lock]() {
                       lock.read_unlock();
                       fmt::print("fiber #1 release lock...\n");
                   });
    futs.emplace_back(std::move(fut));

#if 0
    fut = lock.read_lock()
              .then([]() {
                  fmt::print("fiber #2 get lock...\n");
                  return ss::sleep(3s);
              })
              .then([]() {
                  fmt::print("fiber #2 sleep done...\n");
                  return ss::make_ready_future<>();
              })
              .finally([&lock]() {
                  lock.read_unlock();
                  fmt::print("fiber #2 release lock...\n");
              });
    futs.emplace_back(std::move(fut));

    fut = lock.write_lock()
              .then([]() {
                  fmt::print("fiber #3 get lock...\n");
                  return ss::sleep(1s);
              })
              .then([]() {
                  fmt::print("fiber #3 sleep done...\n");
                  return ss::make_ready_future<>();
              })
              .finally([&lock]() {
                  lock.write_unlock();
                  fmt::print("fiber #3 release lock...\n");
              });
    futs.emplace_back(std::move(fut));
#endif

    fut = ss::sleep(2s)
              .then([&lock]() {
                  fmt::print("fiber #4 sleep done...\n");
                  return lock.read_lock();
              })
              .then_wrapped([](ss::future<>&& fut) {
                  if (fut.failed()) {
                      auto ex = fut.get_exception();
                      fmt::print("fiber #4 fut failed: {}\n", ex);
                      return ss::make_ready_future<>();
                  }
                  fmt::print("fiber #4 get lock...\n");
                  return ss::make_ready_future<>();
              })
              .finally([&lock]() {
                  lock.read_unlock();
                  fmt::print("fiber #4 release lock...\n");
              });
    futs.emplace_back(std::move(fut));

    return ss::when_all(futs.begin(), futs.end())
        .then_wrapped([](ss::future<std::vector<ss::future<>>>&& res) {
            if (res.failed()) {
                fmt::print("ss::when_all failed: {}\n", res.get_exception());
                return ss::make_ready_future<>();
            }

            fmt::print("\n\n");
            auto futs = res.get0();
            for (size_t i = 0; i < futs.size(); i++) {
                auto& fut = futs[i];
                if (fut.failed()) {
                    fmt::print("fiber #{} failed: {}\n", i + 1,
                               fut.get_exception());
                } else {
                    fmt::print("fiber #{} succeeded\n", i + 1);
                }
            }
            return ss::make_ready_future<>();
        });
}