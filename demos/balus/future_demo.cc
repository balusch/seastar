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

int main(int argc, char** argv) {
    ss::app_template app;
    return app.run(argc, argv, []() { return f1(); });
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