#pragma once
#include "mod/input_command.hpp"
#include <limits>

namespace p1 {
inline constexpr size_t noFinishInputLimit = std::numeric_limits<size_t>::max();
inline std::vector<InputCmd> completedPlan(const std::vector<InputCmd>& inputs, size_t applied) {
    return {inputs.begin(), inputs.begin() + std::min(applied, inputs.size())};
}
}
