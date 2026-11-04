#pragma once

#include <string>

// Edit these four switches, then rebuild. true = enabled, false = disabled.
// Exchangeable-parts dominance covers BOTH Type-I and Type-II branches.
namespace AblationConfig {
inline constexpr bool USE_GLOBAL_STATE = false;
inline constexpr bool USE_SAFE_MERGE = false;
inline constexpr bool USE_ADJACENT_INTERCHANGE = false;
inline constexpr bool USE_EXCHANGEABLE_PARTS = false;

// Both bounds are active: the combined lower bound is max(LBpar, LBpos).
inline constexpr bool USE_POSITIONAL_BOUND = true;

inline std::string experiment_tag() {
    return std::string(USE_POSITIONAL_BOUND ? "LBpar_LBpos" : "LBpar")
        + "_G" + (USE_GLOBAL_STATE ? "1" : "0")
        + "_M" + (USE_SAFE_MERGE ? "1" : "0")
        + "_I" + (USE_ADJACENT_INTERCHANGE ? "1" : "0")
        + "_E" + (USE_EXCHANGEABLE_PARTS ? "1" : "0");
}
} // namespace AblationConfig
