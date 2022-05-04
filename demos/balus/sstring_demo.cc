
#include "seastar/core/sstring.hh"

#include <iostream>

namespace ss = seastar;

int main(int argc, char **argv)
{
    {
        ss::sstring s1{"hello"};
        ss::sstring s2{""};
        std::cout << "find: " << s1.find(s2, s2.size()) << std::endl;
    }


    {
        ss::sstring s1 = "hello";
        s1.replace(1, 3, "IO", sizeof("IO") - 1);
        std::cout << "after replace: " << s1 << std::endl;
    }

    {
        ss::sstring s1 = "hello";
        ss::sstring s2{};
        s1.replace(s1.begin() + 1, s1.begin() + 3, s2.begin(), s2.end());
        std::cout << "after replace: " << s1 << std::endl;
    }

    {   // overflow exception?
        ss::sstring s1 = "hello";
        ss::sstring s2 = "nice to meet you";
        s1.replace(s1.begin() + 1, s1.begin() + 3, s2.end(), s2.begin());
        std::cout << "after replace: " << s1 << std::endl;
    }
}
