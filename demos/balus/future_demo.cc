#include <chrono>
#include <iostream>

#include "seastar/core/app-template.hh"
#include "seastar/core/future-util.hh"
#include "seastar/core/future.hh"

namespace ss = seastar;

class Widget {
public:
    explicit Widget(uint64_t& ctor_counter)
        : name_(""), ctor_counter_(&ctor_counter) {}
    Widget(std::string name, uint64_t& ctor_counter)
        : name_(std::move(name)), ctor_counter_(&ctor_counter) {}
    Widget(const Widget& w1)
        : name_(w1.name_),
          local_copy_counter_(0),
          ctor_counter_(&local_copy_counter_) {
        printf("call copy constructor %s\n", name_.c_str());
        ++*w1.ctor_counter_;
    }
    Widget(Widget&& w1) noexcept
        : name_(std::move(w1.name_)),
          local_copy_counter_(0),
          ctor_counter_(&local_copy_counter_) {
        printf("call move constructor %s\n", name_.c_str());
    }

    const std::string& GetName() const noexcept { return name_; }
    uint64_t GetCopyCounter() const noexcept { return *ctor_counter_; }

private:
    std::string name_;
    uint64_t local_copy_counter_{};
    uint64_t* ctor_counter_ = &local_copy_counter_;
};

[[maybe_unused]] static ss::future<> f1();
[[maybe_unused]] static ss::future<> f2();

int main(int argc, char** argv) {
    ss::app_template app;
    return app.run(argc, argv, []() { return f2(); });
}

static ss::future<> f1() {
    uint64_t counter{};
    Widget w1("tom", counter);
    auto fut = ss::make_ready_future<Widget>(std::move(w1));
    auto w2 = fut.get0();
    std::cout << "name: " << w2.GetName() << ", ctor counter: " << counter
              << std::endl;
    return ss::make_ready_future<>();
}

class balus_exception : public std::exception {
public:
    explicit balus_exception(const std::string& msg) : _msg(msg) {}

    virtual const char* what() const throw() { return _msg.c_str(); }

    virtual const std::string& str() const { return _msg; }

private:
    std::string _msg;
};

struct MyObject {
public:
    explicit MyObject(const std::string& name) : _name(name) {
        fmt::print("construct: {}\n", _name);
    }
    MyObject(const MyObject& rhs) : _name(rhs._name) {
        fmt::print("copy construct: {}\n", _name);
    }
    MyObject(MyObject&& rhs) : _name(std::move(rhs._name)) {
        fmt::print("move construct: {}\n", _name);
    }

private:
    std::string _name;
};

// test future::handle_exception_type() method
static ss::future<> f2() {
    MyObject mo("hello");
    return ss::make_exception_future<>(balus_exception("balus: test exception"))
        .handle_exception_type([mo = std::move(mo)](const balus_exception& ex) {
            fmt::print("handle balus_exception: {}\n", ex.str());
        });
}