#pragma once

#include <string>
#include <vector>

#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

struct ValidationError
{
    std::string message;
};

class ValidationModule
{
public:
    ValidationModule() = default;

    [[nodiscard]] bool validate(const TopologyDraft& topology,
        const LayoutDraft& layout,
        std::vector<ValidationError>& out_errors) const;
};

} // namespace genesis::worldgen

