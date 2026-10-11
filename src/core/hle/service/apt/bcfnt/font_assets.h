// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
#include "core/hle/service/apt/bcfnt/font_validation.h"

namespace Service::APT::BCFNT {
void SetFontDirectory(std::string directory);
FontResult LoadBuiltinCjkFont();
FontResult LoadImportedFont(const std::string& hash);
} // namespace Service::APT::BCFNT
