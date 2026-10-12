// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace VideoCore {
inline std::atomic<std::uint64_t> active_shader_compilations{};
inline std::atomic<std::uint64_t> finished_shader_compilations{};

// Observes host shader/pipeline compilation without waiting on the compiler or graphics queue.
class ShaderCompilationScope {
public:
    explicit ShaderCompilationScope(bool enabled = true) : enabled{enabled} {
        if (enabled)
            ++active_shader_compilations;
    }
    ~ShaderCompilationScope() {
        if (enabled) {
            ++finished_shader_compilations;
            --active_shader_compilations;
        }
    }
    ShaderCompilationScope(const ShaderCompilationScope&) = delete;
    ShaderCompilationScope& operator=(const ShaderCompilationScope&) = delete;

private:
    bool enabled;
};
inline std::array<std::uint64_t, 2> GetShaderCompilationStats() {
    return {active_shader_compilations.load(), finished_shader_compilations.load()};
}
} // namespace VideoCore
