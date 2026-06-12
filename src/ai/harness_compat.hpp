/**
 * @file harness_compat.hpp
 * @brief CHRONICLE_ENABLE_HARNESS macro defaulter and shared disabled-build helper.
 *
 * @details The build sets @c CHRONICLE_ENABLE_HARNESS to @c 1 or @c 0 via
 * @c target_compile_definitions; this header defaults it to @c 1 for any
 * translation unit that somehow lacks the definition, and exposes
 * @ref throw_harness_disabled so the disabled-build stubs in
 * @ref HarnessAgentAdapter, @ref NpcAgentPool, and @ref ToolRegistry share one
 * canonical error message.
 */

#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef CHRONICLE_ENABLE_HARNESS
#define CHRONICLE_ENABLE_HARNESS 1
#endif

namespace chronicle {

/// @brief Throw a @c std::runtime_error explaining that Chronicle was built
/// without zoo-keeper-harness support.
///
/// @param caller Name of the calling function, included in the error message
///               so users can identify which AI entry point they hit.
[[noreturn]] inline void throw_harness_disabled(std::string_view caller) {
    throw std::runtime_error(std::string(caller) +
                             ": Chronicle was built without zoo-keeper-harness support. "
                             "Reconfigure with -DCHRONICLE_ENABLE_HARNESS=ON to use "
                             "LLM endpoint inference.");
}

} // namespace chronicle
