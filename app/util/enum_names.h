#pragma once

#include <magic_enum/magic_enum.hpp>

template<typename Enum>
inline const char* enum_name_or_unknown(Enum value) noexcept {
    const auto name = magic_enum::enum_name(value);
    return name.empty() ? "???" : name.data();
}
