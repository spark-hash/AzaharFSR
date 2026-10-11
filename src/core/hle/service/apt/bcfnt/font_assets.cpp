// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <fstream>
#include <mutex>
#include "cjk_font.bcfnt.zstd.h"
#include "common/file_util.h"
#include "common/zstd_compression.h"
#include "core/hle/service/apt/bcfnt/font_assets.h"

namespace Service::APT::BCFNT {
namespace {
std::mutex directory_mutex;
std::string font_directory;
} // namespace
void SetFontDirectory(std::string directory) {
    std::scoped_lock lock{directory_mutex};
    font_directory = std::move(directory);
}
FontResult LoadBuiltinCjkFont() {
    if (Common::Compression::GetDecompressedSize(CJK_FONT_ZSTD) > SharedFontMemorySize - 0x80)
        return {{}, "Builtin CJK font exceeds shared memory"};
    return NormalizeFont(Common::Compression::DecompressDataZSTD(CJK_FONT_ZSTD));
}
FontResult LoadImportedFont(const std::string& hash) {
    if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }))
        return {{}, "No imported font selected"};
    std::string directory;
    {
        std::scoped_lock lock{directory_mutex};
        directory = font_directory;
    }
    if (directory.empty())
        directory = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + "fonts";
    std::ifstream stream(directory + "/" + hash + ".bcfnt", std::ios::binary | std::ios::ate);
    const auto size = stream.tellg();
    if (size < 20 || size > static_cast<std::streamoff>(SharedFontMemorySize))
        return {{}, "Imported font file missing or too large"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size))
        return {{}, "Unable to read imported font"};
    return NormalizeFont(bytes);
}
} // namespace Service::APT::BCFNT
