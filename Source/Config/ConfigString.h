#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

template <std::size_t Capacity>
struct ConfigString {
    constexpr ConfigString() noexcept = default;

    constexpr ConfigString(const char* str) noexcept
    {
        assign(str);
    }

    constexpr void assign(const char* str) noexcept
    {
        size = 0;
        if (!str) {
            value[0] = '\0';
            return;
        }

        while (str[size] != '\0' && size < Capacity - 1) {
            value[size] = str[size];
            ++size;
        }
        value[size] = '\0';
    }

    constexpr void assign(std::string_view str) noexcept
    {
        size = (std::min)(str.size(), Capacity - 1);
        std::copy_n(str.data(), size, value);
        value[size] = '\0';
    }

    [[nodiscard]] constexpr const char* c_str() const noexcept
    {
        return value;
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return {value, size};
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return size == 0;
    }

    [[nodiscard]] friend constexpr bool operator==(const ConfigString& lhs, const ConfigString& rhs) noexcept
    {
        return lhs.view() == rhs.view();
    }

    char value[Capacity]{};
    std::size_t size{};
};
