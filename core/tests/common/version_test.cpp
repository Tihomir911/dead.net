#include "deadnet/common/version.hpp"

#include <iostream>
#include <string>

int main() {
    using namespace deadnet::common;

    if (kVersion.major != 0) {
        std::cerr << "Version major mismatch\n";
        return 1;
    }

    if (kVersion.minor != 1) {
        std::cerr << "Version minor mismatch\n";
        return 1;
    }

    if (kVersion.patch != 0) {
        std::cerr << "Version patch mismatch\n";
        return 1;
    }

    if (versionString() != "0.1.0") {
        std::cerr << "Version string mismatch\n";
        return 1;
    }

    std::cout << "DEAD.NET Core version: "
              << versionString() << '\n';

    std::cout << "All tests passed!\n";

    return 0;
}