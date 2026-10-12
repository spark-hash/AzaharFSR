// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/shader_compilation_stats.h"
#include <iostream>
#include <stdexcept>

int main() {
  const auto check = [](bool value) {
    if (!value)
      throw std::runtime_error("Shader compilation telemetry failed");
  };
  using VideoCore::GetShaderCompilationStats;
  using VideoCore::ShaderCompilationScope;
  check(GetShaderCompilationStats() == std::array<std::uint64_t, 2>{0, 0});
  {
    ShaderCompilationScope cached{false};
    check(GetShaderCompilationStats()[0] == 0);
  }
  {
    ShaderCompilationScope first;
    check(GetShaderCompilationStats()[0] == 1);
    {
      ShaderCompilationScope second;
      check(GetShaderCompilationStats()[0] == 2);
    }
    check(GetShaderCompilationStats() == std::array<std::uint64_t, 2>{1, 1});
  }
  check(GetShaderCompilationStats() == std::array<std::uint64_t, 2>{0, 2});
  try {
    ShaderCompilationScope failing;
    throw std::runtime_error("compiler failure");
  } catch (const std::runtime_error &) {
  }
  check(GetShaderCompilationStats() == std::array<std::uint64_t, 2>{0, 3});
  std::cout << "7 shader compilation telemetry checks passed\n";
}
