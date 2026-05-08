/**
 * @file zoo_compat.hpp
 * @brief CHRONICLE_ENABLE_ZOO macro defaulter and shared "no zoo" stub helper.
 *
 * @details The build sets @c CHRONICLE_ENABLE_ZOO to @c 1 or @c 0 via
 * @c target_compile_definitions; this header defaults it to @c 1 for any
 * translation unit that somehow lacks the definition, and exposes
 * @ref throw_zoo_disabled so the disabled-build stubs in
 * @ref ZooAgentAdapter, @ref NpcAgentPool, and @ref ToolRegistry share one
 * canonical error message.
 */

#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef CHRONICLE_ENABLE_ZOO
#define CHRONICLE_ENABLE_ZOO 1
#endif

namespace chronicle {

/// @brief Throw a @c std::runtime_error explaining that Chronicle was built
/// without Zoo-Keeper support.
///
/// @param caller Name of the calling function, included in the error message
///               so users can identify which AI entry point they hit.
[[noreturn]] inline void throw_zoo_disabled(std::string_view caller) {
    throw std::runtime_error(std::string(caller) +
                             ": Chronicle was built without Zoo-Keeper support. "
                             "Reconfigure with -DCHRONICLE_ENABLE_ZOO=ON to use "
                             "local model inference.");
}

} // namespace chronicle
