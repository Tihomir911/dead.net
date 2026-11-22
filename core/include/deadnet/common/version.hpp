#pragma once

#include <cstdint>
#include <string>

namespace deadnet::common{
    struct Version {

        std::uint16_t major;
        std::uint16_t minor;
        std::uint16_t patch;
    };

    inline constexpr Version kVersion{
        0,
        1,
        0
    };

    [[nodiscard]] std::string versionString();
}