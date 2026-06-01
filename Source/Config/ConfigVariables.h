#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

#include "ConfigVariable.h"
#include "ConfigVariableTypes.h"
#include <Utils/Meta.h>

class ConfigVariables {
public:
    ConfigVariables() noexcept
    {
        ConfigVariableTypes::forEach([this] <typename ConfigVariable> (std::type_identity<ConfigVariable>) {
            storeVariableValue<ConfigVariable>(ConfigVariable::kDefaultValue);
        });
    }

    template <typename ConfigVariable>
    [[nodiscard]] auto getVariableValue() noexcept
    {
        std::array<std::byte, sizeof(typename ConfigVariable::ValueType)> bytes;
        std::memcpy(bytes.data(), &variableStorage<ConfigVariable>(), bytes.size());
        return std::bit_cast<typename ConfigVariable::ValueType>(bytes);
    }

    template <typename ConfigVariable>
    void storeVariableValue(ConfigVariable::ValueType value) noexcept
    {
        std::memcpy(&variableStorage<ConfigVariable>(), &value, sizeof(value));
    }

private:
    template <typename ConfigVariable>
    [[nodiscard]] auto& variableStorage() noexcept
    {
        if constexpr (OneByteConfigVariables::contains<ConfigVariable>())
            return oneByteConfigVariables[OneByteConfigVariables::template indexOf<ConfigVariable>()];
        else if constexpr (TwoByteConfigVariables::contains<ConfigVariable>())
            return twoByteConfigVariables[TwoByteConfigVariables::template indexOf<ConfigVariable>()];
        else if constexpr (LargeConfigVariables::contains<ConfigVariable>())
            return largeConfigVariables[LargeConfigVariables::template indexOf<ConfigVariable>()];
        else
            static_assert(!std::is_same_v<ConfigVariable, ConfigVariable>, "Unknown type");
    }

    using OneByteConfigVariables = ConfigVariableTypes::filter<Projected<UnpackConfigVariable, WithSizeOf<1>::Equal>::Value>;
    using TwoByteConfigVariables = ConfigVariableTypes::filter<Projected<UnpackConfigVariable, WithSizeOf<2>::Equal>::Value>;
    using LargeConfigVariables = ConfigVariableTypes::filter<Projected<UnpackConfigVariable, WithSizeOf<2>::Greater>::Value>;

    std::array<std::byte[1], OneByteConfigVariables::size()> oneByteConfigVariables;
    std::array<std::byte[2], TwoByteConfigVariables::size()> twoByteConfigVariables;
    std::array<std::byte[LargeConfigVariables::template max<Projected<UnpackConfigVariable, SizeOf>::Value>()], LargeConfigVariables::size()> largeConfigVariables;
};
