// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <set>
#include "core/hle/service/apt/bcfnt/font_validation.h"

namespace Service::APT::BCFNT {
FontResult NormalizeFont(std::span<const std::uint8_t> input) {
    const auto fail = [](const char* error) { return FontResult{{}, error}; };
    if (input.size() > SharedFontMemorySize || input.size() < 20)
        return fail("Font exceeds 3DS shared memory or is truncated");
    if (std::memcmp(input.data(), "CFNT", 4) != 0 && std::memcmp(input.data(), "CFNU", 4) != 0 &&
        input.size() >= 0x94 &&
        (std::memcmp(input.data() + 0x80, "CFNT", 4) == 0 ||
         std::memcmp(input.data() + 0x80, "CFNU", 4) == 0))
        input = input.subspan(0x80);
    if ((std::memcmp(input.data(), "CFNT", 4) != 0 && std::memcmp(input.data(), "CFNU", 4) != 0))
        return fail("Select an uncompressed BCFNT or shared_font.bin, not a CIA/TTF");
    const auto u16 = [&](std::size_t p) -> std::uint16_t {
        return input[p] | (static_cast<std::uint16_t>(input[p + 1]) << 8);
    };
    const auto u32 = [&](std::size_t p) -> std::uint32_t {
        return u16(p) | (static_cast<std::uint32_t>(u16(p + 2)) << 16);
    };
    const auto size = u32(12);
    if (u16(4) != 0xfeff || u16(6) != 20 || u32(8) != 0x03000000 || size > input.size() ||
        size < 20 || size > SharedFontMemorySize - 0x80)
        return fail("Unsupported BCFNT header, version or size");
    input = input.first(size);
    const auto blocks = u32(16);
    if (blocks < 4 || blocks > size / 8 || blocks > 4096)
        return fail("Invalid font section count");
    std::map<std::size_t, std::string> sections;
    std::map<std::string, std::size_t> first;
    std::size_t pos = 20;
    for (std::uint32_t i = 0; i < blocks; ++i) {
        if (size - pos < 8)
            return fail("Truncated font section header");
        const std::string tag(reinterpret_cast<const char*>(input.data() + pos), 4);
        const auto length = u32(pos + 4);
        const auto minimum = tag == "FINF" || tag == "TGLP" ? 32 : tag == "CWDH" ? 16 : 20;
        if ((tag != "FINF" && tag != "TGLP" && tag != "CWDH" && tag != "CMAP") ||
            length < static_cast<unsigned>(minimum) || length > size - pos)
            return fail("Invalid font section bounds or type");
        if ((tag == "FINF" || tag == "TGLP") && first.contains(tag))
            return fail("Duplicate font information or texture section");
        sections.emplace(pos, tag);
        first.try_emplace(tag, pos);
        pos += length;
    }
    if (pos != size || first.size() != 4)
        return fail("Incomplete BCFNT sections");
    const auto finf = first.at("FINF"), tglp = first.at("TGLP");
    if (input[finf + 15] != 1 || !input[finf + 28] || !input[finf + 29])
        return fail("Font must use UTF-16 with nonzero character dimensions");
    const std::uint32_t base = u32(finf + 16) - static_cast<std::uint32_t>(tglp + 8);
    std::map<std::size_t, std::uint32_t> fixups;
    const auto pointer = [&](std::size_t field, const std::string& tag, bool nullable) {
        const auto value = u32(field);
        if (!value && nullable)
            return true;
        const auto offset = value - base;
        if (offset < 8 || !sections.contains(offset - 8) || sections.at(offset - 8) != tag)
            return false;
        fixups[field] = offset;
        return true;
    };
    if (!pointer(finf + 16, "TGLP", false) || !pointer(finf + 20, "CWDH", false) ||
        !pointer(finf + 24, "CMAP", false) || u32(finf + 20) - base != first.at("CWDH") + 8 ||
        u32(finf + 24) - base != first.at("CMAP") + 8)
        return fail("Invalid or inconsistent BCFNT section pointers");
    const std::uint64_t sheets = u16(tglp + 16), columns = u16(tglp + 20), rows = u16(tglp + 22);
    const auto width = u16(tglp + 24), height = u16(tglp + 26), format = u16(tglp + 18);
    constexpr std::array<unsigned, 14> bits{32, 24, 16, 16, 16, 16, 16, 8, 8, 8, 4, 4, 4, 8};
    const auto texture = u32(tglp + 28) - base;
    const std::uint64_t texture_size = sheets * u32(tglp + 12);
    if (!sheets || !columns || !rows || !width || !height || width > 1024 || height > 1024 ||
        (width & (width - 1)) || (height & (height - 1)) || format >= bits.size() ||
        u32(tglp + 12) != static_cast<unsigned>(width) * height * bits[format] / 8 ||
        columns * (input[tglp + 8] + 1) > width || rows * (input[tglp + 9] + 1) > height ||
        texture < tglp + 32 || texture > tglp + u32(tglp + 4) ||
        texture_size > tglp + u32(tglp + 4) - texture)
        return fail("Invalid font texture dimensions, storage or pointer");
    fixups[tglp + 28] = texture;
    const std::uint64_t capacity = sheets * columns * rows;
    std::set<unsigned> widths;
    for (const auto& [section, tag] : sections) {
        const auto length = u32(section + 4);
        if (tag == "CWDH") {
            const auto start = u16(section + 8), end = u16(section + 10);
            if (end < start || end >= capacity || 16 + (end - start + 1) * 3u > length ||
                !pointer(section + 12, "CWDH", true))
                return fail("Invalid character widths");
            for (unsigned glyph = start; glyph <= end; ++glyph)
                if (!widths.insert(glyph).second)
                    return fail("Overlapping character widths");
        } else if (tag == "CMAP") {
            const auto start = u16(section + 8), end = u16(section + 10),
                       method = u16(section + 12);
            if (end < start || method > 2 || !pointer(section + 16, "CMAP", true))
                return fail("Invalid character mapping");
            if (method == 0 && (length < 22 || u16(section + 20) + end - start >= capacity))
                return fail("Invalid direct mapping");
            if (method == 1 && 20 + (end - start + 1) * 2u > length)
                return fail("Truncated mapping table");
            if (method == 1) {
                for (unsigned j = 0; j <= static_cast<unsigned>(end - start); ++j) {
                    const auto glyph = u16(section + 20 + j * 2);
                    if (glyph != 0xffff && glyph >= capacity)
                        return fail("Invalid mapping table glyph");
                }
            }
            if (method == 2) {
                if (length < 22 || 22 + u16(section + 20) * 4u > length)
                    return fail("Truncated character map");
                unsigned previous = 0;
                for (unsigned j = 0; j < u16(section + 20); ++j) {
                    const auto code = u16(section + 22 + j * 4), glyph = u16(section + 24 + j * 4);
                    if (code < start || code > end || (j && code <= previous) || glyph >= capacity)
                        return fail("Invalid character map entry");
                    previous = code;
                }
            }
        }
    }
    if (!widths.contains(u16(finf + 10)))
        return fail("Missing fallback glyph widths");
    for (const std::string tag : {"CWDH", "CMAP"}) {
        std::set<std::size_t> seen;
        auto section = first.at(tag);
        while (true) {
            if (!seen.insert(section).second)
                return fail("Cyclic font section chain");
            const auto field = section + (tag == "CWDH" ? 12 : 16);
            if (!u32(field))
                break;
            section = fixups.at(field) - 8;
        }
        for (const auto& [offset, section_tag] : sections)
            if (section_tag == tag && !seen.contains(offset))
                return fail("Unlinked font section");
    }
    FontResult result{std::vector<std::uint8_t>(input.begin(), input.end()), {}};
    std::memcpy(result.data.data(), "CFNT", 4);
    for (const auto& [field, value] : fixups)
        for (unsigned byte = 0; byte < 4; ++byte)
            result.data[field + byte] = value >> (byte * 8);
    return result;
}
} // namespace Service::APT::BCFNT
