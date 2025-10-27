#pragma once

#include <string>

namespace genesis::agents::components {

// 仅保留与身份相关的组件；移动/位置改用 Movement2D.hpp 中的 2D 组件。
struct AgentName {
    std::string name;
};

} // namespace genesis::agents::components

