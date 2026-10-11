// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <string>

namespace Vulkan {
void SetLsfgShaderDirectory(std::string path);
std::string GetLsfgShaderDirectory();
// Returns an empty string on success. Never executes the DLL.
std::string ImportLsfgShaders(const std::string& dll, const std::string& output);
// Real and generated presentations, generated processing ms, status.
std::array<double, 4> GetLsfgStats();
void CountLsfgPresent(bool generated);
void SetLsfgStatus(int state, double milliseconds = 0);
} // namespace Vulkan
