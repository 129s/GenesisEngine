#pragma once

#include <cstdint>

namespace genesis::base {

// 简易语义化版本结构，预留基础层通用类型示例。
struct SemVer {
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::uint32_t patch{0};
};

} // namespace genesis::base

