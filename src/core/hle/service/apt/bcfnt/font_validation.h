// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Service::APT::BCFNT {
constexpr std::size_t SharedFontMemorySize = 0x332000;
struct FontResult {
    std::vector<std::uint8_t> data;
    std::string error;
};
// Accepts uncompressed BCFNT or a shared_font.bin dump; returns unrelocated BCFNT.
FontResult NormalizeFont(std::span<const std::uint8_t> input);
} // namespace Service::APT::BCFNT
