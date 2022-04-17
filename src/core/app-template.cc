/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 */

#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/alien.hh>
#include <seastar/core/scollectd.hh>
#include <seastar/core/metrics_api.hh>
#include <boost/program_options.hpp>
#include <seastar/core/print.hh>
#include <seastar/util/log.hh>
#include <seastar/util/log-cli.hh>
#include <seastar/net/native-stack.hh>
#include <boost/program_options.hpp>
#include <boost/make_shared.hpp>
#include <fstream>
#include <cstdlib>

#include "program_options.hh"

namespace seastar {

namespace bpo = boost::program_options;

using namespace std::chrono_literals;

static
app_template::seastar_options
seastar_options_from_config(app_template::config cfg) {
    app_template::seastar_options opts;
    opts.name = std::move(cfg.name);
    opts.description = std::move(cfg.description);
    opts.auto_handle_sigint_sigterm = std::move(cfg.auto_handle_sigint_sigterm);
    opts.reactor_opts.task_quota_ms.set_default_value(cfg.default_task_quota / 1ms);
    opts.reactor_opts.max_networking_io_control_blocks.set_default_value(cfg.max_networking_aio_io_control_blocks);
    return opts;
}

app_template::seastar_options::seastar_options()
    : program_options::option_group(nullptr, "seastar")
    , reactor_opts(this)
    , metrics_opts(this)
    , smp_opts(this)
    , scollectd_opts(this)
    , log_opts(this)
{
}

app_template::app_template(app_template::seastar_options opts)
    : _alien(std::make_unique<alien::instance>())
    , _smp(std::make_shared<smp>(*_alien))
    , _opts(std::move(opts))
    , _app_opts(_opts.name + " options")
    , _conf_reader(get_default_configuration_reader()) {

        if (!alien::internal::default_instance) {
            alien::internal::default_instance = _alien.get();
        }
        _app_opts.add_options()
                ("help,h", "show help message")
                ;
        _app_opts.add_options()
                ("help-seastar", "show help message about seastar options")
                ;
        _app_opts.add_options()
                ("help-loggers", "print a list of logger names and exit")
                ;

        {
            program_options::options_description_building_visitor visitor;
            _opts.describe(visitor);
            _opts_conf_file.add(std::move(visitor).get_options_description());
        }

        _seastar_opts.add(_opts_conf_file);
}

app_template::app_template(app_template::config cfg)
    : app_template(seastar_options_from_config(std::move(cfg)))
{
}

app_template::~app_template() = default;

const app_template::seastar_options& app_template::options() const {
    return _opts;
}

app_template::configuration_reader app_template::get_default_configuration_reader() {
    return [this] (bpo::variables_map& configuration) {
        auto home = std::getenv("HOME");
        if (home) {
            std::ifstream ifs(std::string(home) + "/.config/seastar/seastar.conf");
            if (ifs) {
                bpo::store(bpo::parse_config_file(ifs, _opts_conf_file), configuration);
            }
            std::ifstream ifs_io(std::string(home) + "/.config/seastar/io.conf");
            if (ifs_io) {
                bpo::store(bpo::parse_config_file(ifs_io, _opts_conf_file), configuration);
            }
        }
    };
}

void app_template::set_configuration_reader(configuration_reader conf_reader) {
    _conf_reader = conf_reader;
}

boost::program_options::options_description& app_template::get_options_description() {
    return _app_opts;
}

boost::program_options::options_description& app_template::get_conf_file_options_description() {
    return _opts_conf_file;
}

boost::program_options::options_description_easy_init
app_template::add_options() {
    return _app_opts.add_options();
}

void
app_template::add_positional_options(std::initializer_list<positional_option> options) {
    for (auto&& o : options) {
        _app_opts.add(boost::make_shared<bpo::option_description>(o.name, o.value_semantic, o.help));
        _pos_opts.add(o.name, o.max_count);
    }
}


bpo::variables_map&
app_template::configuration() {
    return *_configuration;
}

int
app_template::run(int ac, char ** av, std::function<future<int> ()>&& func) noexcept {
    return run_deprecated(ac, av, [func = std::move(func)] () mutable {
        /* balus(Q): 这块代码没有看明白
         * - 为什么要 at_exit 这样一个函数呢？
         * - 为什么 finally 没有接受一个 int exit_code 参数，反而是后面串联的 then 接受的呢？
         * 需要去理解 Seastar 中的 finally 的语义：
         * .finally(): similar to a Java finally block, a .finally() continuation is
         * executed whether or not its input future carries an exception or not. The
         * result of the finally continuation is its input future, so .finally() can
         * be used to insert code in a flow that is executed unconditionally, but
         * otherwise does not alter the flow.
         * 也就是说 finally 并不是只能用作最后一个 continuation，而只是一个**一定会执行**的
         * continuation，它返回的还是调用它的 future，这就相当于在 continuation chain(即
         * fiber) 中插入一段代码，但是并不影响整个 fiber 的执行；比如在两个 continuation A
         * 和 B 中插入一个 finally block，并不会影响 B 的 input future(依旧是 A)
         */

        auto func_done = make_lw_shared<promise<>>();
        engine().at_exit([func_done] { return func_done->get_future(); });
        // No need to wait for this future.
        // func's returned exit_code is communicated via engine().exit()
        (void)futurize_invoke(func).finally([func_done] {
            func_done->set_value();
        }).then([] (int exit_code) {
            return engine().exit(exit_code);
        }).or_terminate();
    });
}

int
app_template::run(int ac, char ** av, std::function<future<> ()>&& func) noexcept {
    return run(ac, av, [func = std::move(func)] {
        return func().then([] () {
            return 0;
        });
    });
}

int
app_template::run_deprecated(int ac, char ** av, std::function<void ()>&& func) noexcept {
#ifdef SEASTAR_DEBUG
    fmt::print(std::cerr, "WARNING: debug mode. Not for benchmarking or production\n");
#endif
    boost::program_options::options_description all_opts;
    all_opts.add(_app_opts);
    all_opts.add(_seastar_opts);

    bpo::variables_map configuration;
    try {
        bpo::store(bpo::command_line_parser(ac, av)
                    .options(all_opts)
                    .positional(_pos_opts)
                    .run()
            , configuration);
        _conf_reader(configuration);
    } catch (bpo::error& e) {
        fmt::print("error: {}\n\nTry --help.\n", e.what());
        return 2;
    }
    if (configuration.count("help")) {
        if (!_opts.description.empty()) {
            std::cout << _opts.description << "\n";
        }
        std::cout << _app_opts << "\n";
        return 1;
    }
    if (configuration.count("help-seastar")) {
        std::cout << _seastar_opts << "\n";
        return 1;
    }
    if (configuration.count("help-loggers")) {
        log_cli::print_available_loggers(std::cout);
        return 1;
    }

    try {
        bpo::notify(configuration);
    } catch (const bpo::error& ex) {
        std::cout << ex.what() << std::endl;
        return 1;
    }

    {
        program_options::variables_map_extracting_visitor visitor(configuration);
        _opts.mutate(visitor);
    }
    _opts.reactor_opts._argv0 = std::string(av[0]);
    _opts.reactor_opts._auto_handle_sigint_sigterm = _opts.auto_handle_sigint_sigterm;
    if (auto* native_stack = dynamic_cast<net::native_stack_options*>(_opts.reactor_opts.network_stack.get_selected_candidate_opts())) {
        native_stack->_hugepages = _opts.smp_opts.hugepages;
    }

    // Needs to be before `smp::configure()`.
    try {
        apply_logging_settings(log_cli::extract_settings(_opts.log_opts));
    } catch (const std::runtime_error& exn) {
        std::cout << "logging configuration error: " << exn.what() << '\n';
        return 1;
    }

    /* balus(N): 在 _smp->configure() 中就创建了所有的 reactor，除 cpu-0 上的 reactor
     * 之外的所有 reactor 都直接进入了工作循环，而 reactor-0 作为管理线程还需要处理其他事宜 */

    try {
        _smp->configure(_opts.smp_opts, _opts.reactor_opts);
    } catch (...) {
        std::cerr << "Could not initialize seastar: " << std::current_exception() << std::endl;
        return 1;
    }
    _configuration = {std::move(configuration)};
    // No need to wait for this future.
    // func is waited on via engine().run()
    (void)engine().when_started().then([this] {
        return seastar::metrics::configure(_opts.metrics_opts).then([this] {
            // set scollectd use the metrics configuration, so the later
            // need to be set first
            scollectd::configure( _opts.scollectd_opts);
        });
    }).then(
        /* balus(N): 真正调用函数的地方在这里，虽然没有 func() 这样显式调用，
         * 但是实际上 then 方法就是接受一个 callable object 然后调用，看惯了
         * 直接构造 lambda 的现在来看这种传入一个已有的 callable object 会感觉有点奇怪 */
        std::move(func)
    ).then_wrapped([] (auto&& f) {
        try {
            f.get();
        } catch (std::exception& ex) {
            std::cout << "program failed with uncaught exception: " << ex.what() << "\n";
            engine().exit(1);
        }
    });

    /* reactor-0 也进入了其 run loop，主要是接受一些外部指令，以及其他 reactor 发来的任务(比如退出任务)
     * 从这里也可以发现，传给 app-template 的函数是在 reactor-0 上被执行的，但是这并不意味着它只用到了
     * reactor-0，因为在其中可以通过 smp 类将任务发送到其他 reactor 去执行从而将其他 cpu 也利用起来 */
    auto exit_code = engine().run();
    _smp->cleanup();
    return exit_code;
}

}
