#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/os.h>

int main(int argc, char** argv)
{
    fmt::println("{0} {1} {0}", "Hello", "World");

    fmt::println("{:3.2f}", 123.4567);

    return 0;
}
