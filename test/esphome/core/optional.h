#pragma once

#include <optional>

// Fakes the esphome optional type with std::optional (C++17)

namespace esphome
{
    template <typename T>
    using optional = std::optional<T>;
}
