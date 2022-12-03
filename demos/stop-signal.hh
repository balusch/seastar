#pragma once

#include <seastar/core/abort_source.hh>
#include <seastar/core/condition-variable.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>

namespace ss = seastar;

class stop_signal {
public:
  stop_signal() {
    ss::engine().handle_signal(SIGINT, [this] {
      std::cout << "handle SIGNIT" << std::endl;
      signaled();
    });
    ss::engine().handle_signal(SIGTERM, [this] {
      std::cout << "handle SIGTERM" << std::endl;
      signaled();
    });
  }
  ~stop_signal() {
    std::cout << "destruct stop signal";
    ss::engine().handle_signal(
        SIGINT, [] { std::cout << "ignore SIGINT" << std::endl; });
    ss::engine().handle_signal(
        SIGTERM, [] { std::cout << "ignore SIGTERM" << std::endl; });

    // We should signal for unit tests, because some background tasks does
    // not finish without it
    signaled();
  }
  ss::future<> wait() {
    return _cond.wait([this] { return _as.abort_requested(); });
  }

  bool stopping() const { return _as.abort_requested(); }

  ss::abort_source &abort_source() { return _as; };

private:
  void signaled() {
    if (!_as.abort_requested()) {
      _as.request_abort();
    }
    _cond.broadcast();
  }

  ss::condition_variable _cond;
  ss::abort_source _as;
};
