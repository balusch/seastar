
#include "./stop-signal.hh"

#include "seastar/core/future.hh"
#include "seastar/core/shared_ptr.hh"
#include "seastar/core/smp.hh"
#include "seastar/core/thread.hh"
#include "seastar/http/handlers.hh"
#include "seastar/http/reply.hh"
#include "seastar/net/api.hh"
#include "seastar/util/defer.hh"
#include <seastar/core/app-template.hh>
#include <seastar/core/sharded.hh>
#include <seastar/http/httpd.hh>
#include <seastar/util/log.hh>

seastar::logger logger("sharded_demo");

namespace ss = seastar;

class my_handler : public ss::httpd::handler_base {
public:
  my_handler() = default;
  ~my_handler() override = default;

  virtual ss::future<std::unique_ptr<ss::http::reply>>
  handle(const ss::sstring &path, std::unique_ptr<ss::http::request> req,
         std::unique_ptr<ss::http::reply> rep) override {
    logger.info("received connection in shard-{}", ss::this_shard_id());
    rep->add_header("hello", "sharded");
    ss::sstring body = "Hello, Seastar!";
    rep->write_body("txt", std::move(body));
    return ss::make_ready_future<std::unique_ptr<ss::http::reply>>(
        std::move(rep));
  }
};

int main(int argc, char **argv) {
  ss::app_template app;
  return app.run(argc, argv, []() {
    return ss::async([]() {
      stop_signal app_signal;
      ss::sharded<ss::httpd::http_server> server;
      auto deferred = ss::defer([&server]() noexcept {
        logger.info("destruct deferred_action");
        server.stop().get0();
      });

      ss::sstring name = "Seastar httpd server";
      server.start(std::move(name)).get0();
      server
          .invoke_on_all([](ss::httpd::http_server &s) {
            logger.info("hello, world on reactor-{}", ss::this_shard_id());
          })
          .get0();
      logger.info("SUCCEED to create HTTP server");

      uint16_t port = 12345;
      ss::socket_address sa(port);
      ss::listen_options lo;
      lo.lba = ss::server_socket::load_balancing_algorithm::port;
      server
          .invoke_on_all<ss::future<> (ss::httpd::http_server::*)(
              ss::socket_address, ss::listen_options)>(
              &ss::httpd::http_server::listen, sa, lo)
          .get0();

      logger.info("SUCCEED to listen port");

      auto handler = new my_handler();
      server
          .invoke_on_all([handler](ss::httpd::http_server &s) {
            s._routes.add_default_handler(handler);
          })
          .get0();

      logger.info("SUCCEED to register handler");

      app_signal.wait().get0();
      logger.info("after stop_signal::wait()");
      return 0;
    });

    return ss::async([]() {
      ss::sharded<ss::httpd::http_server> server;
      auto deferred = ss::defer([&server]() noexcept {
        logger.info("destruct deferred_action");
        server.stop().get0();
      });

      ss::sstring name = "Seastar httpd server";
      server.start(std::move(name)).get0();
      server.invoke_on_all([](ss::httpd::http_server &s) {}).get0();

      uint16_t port = 12345;
      ss::socket_address sa(port);
      server
          .invoke_on_all<ss::future<> (ss::httpd::http_server::*)(
              ss::socket_address)>(&ss::httpd::http_server::listen, sa)
          .get0();

      auto handler = new my_handler();
      server
          .invoke_on_all([handler](ss::httpd::http_server &s) {
            s._routes.add_default_handler(handler);
          })
          .get0();

      return 0;
    });
  });
}

template <typename Service>
template <typename... Args>
future<> sharded<Service>::start(Args &&...args) noexcept {
  return sharded_parallel_for_each(
      [this, args = std::make_tuple(std::forward<Args>(args)...)](
          unsigned c) mutable {
        return smp::submit_to(c, [this, args]() mutable {
          _instances[this_shard_id()].service = std::apply(
              [this](Args... args) {
                return create_local_service(std::forward<Args>(args)...);
              },
              args);
        });
      });
}

template <typename Service>
template <typename Func, typename... Args>
SEASTAR_CONCEPT(requires std::invocable<Func, Service &,
                                        internal::sharded_unwrap_t<Args>...>)
inline future<> sharded<Service>::invoke_on_all(smp_submit_to_options options,
                                                Func func,
                                                Args... args) noexcept {
  static_assert(
      std::is_same_v<futurize_t<std::invoke_result_t<
                         Func, Service &, internal::sharded_unwrap_t<Args>...>>,
                     future<>>,
      "invoke_on_all()'s func must return void or future<>");

  return invoke_on_all(
      options, invoke_on_all_func_type([func = std::move(func),
                                        args = std::tuple(std::move(args)...)](
                                           Service &service) mutable {
        return std::apply(
            [&service, &func](Args &&...args) mutable {
              return futurize_apply(
                  func, std::tuple_cat(std::forward_as_tuple(service),
                                       std::tuple(internal::unwrap_sharded_arg(
                                           std::forward<Args>(args))...)));
            },
            std::move(args));
      }));
}