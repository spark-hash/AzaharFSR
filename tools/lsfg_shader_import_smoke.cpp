// SPDX-License-Identifier: GPL-2.0-or-later
// Headless importer check. Link lsfg_assets.cpp, trans.cpp, dxbc and pe-parse.
#include "video_core/renderer_vulkan/lsfg_assets.h"
#include <filesystem>
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const auto error = Vulkan::ImportLsfgShaders(argv[1], argv[2]);
  if (!error.empty()) {
    std::cerr << error << '\n';
    return 1;
  }
  unsigned count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(argv[2]))
    if (entry.path().extension() == ".spv")
      ++count;
  std::cout << count << " shaders imported\n";
  return count == 26 ? 0 : 1;
}
