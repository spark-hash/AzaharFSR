// SPDX-License-Identifier: GPL-2.0-or-later
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>
#include <pe-parse/parse.h>
#include "extract/trans.hpp"
#include "video_core/renderer_vulkan/lsfg_assets.h"

namespace Vulkan {
namespace {
std::mutex asset_mutex;
std::string shader_directory;
std::atomic<unsigned long long> real_presentations{}, generated_presentations{};
std::atomic<double> generation_ms{};
std::atomic<int> status{};
// Resource numbers used by LSFG 3.1 Performance's FP32 shader chain.
const std::map<std::string, unsigned> shaders{
    {"p_mipmaps", 255},  {"p_generate", 256}, {"p_alpha[0]", 290}, {"p_alpha[1]", 291},
    {"p_alpha[2]", 292}, {"p_alpha[3]", 293}, {"p_beta[0]", 298},  {"p_beta[1]", 299},
    {"p_beta[2]", 300},  {"p_beta[3]", 301},  {"p_beta[4]", 302},  {"p_gamma[0]", 280},
    {"p_gamma[1]", 282}, {"p_gamma[2]", 283}, {"p_gamma[3]", 284}, {"p_gamma[4]", 285},
    {"p_delta[0]", 280}, {"p_delta[1]", 286}, {"p_delta[2]", 287}, {"p_delta[3]", 288},
    {"p_delta[4]", 289}, {"p_delta[5]", 281}, {"p_delta[6]", 294}, {"p_delta[7]", 295},
    {"p_delta[8]", 296}, {"p_delta[9]", 297}};
using Resources = std::map<unsigned, std::vector<uint8_t>>;
int Resource(void* data, const peparse::resource& resource) {
    if (resource.type == peparse::RT_RCDATA && resource.buf && resource.buf->bufLen > 0 &&
        resource.buf->bufLen <= 4 * 1024 * 1024) {
        auto& entries = *static_cast<Resources*>(data);
        entries.try_emplace(resource.name, resource.buf->buf,
                            resource.buf->buf + resource.buf->bufLen);
    }
    return 0;
}
} // namespace

void SetLsfgShaderDirectory(std::string path) {
    std::scoped_lock lock{asset_mutex};
    shader_directory = std::move(path);
}
std::string GetLsfgShaderDirectory() {
    std::scoped_lock lock{asset_mutex};
    return shader_directory;
}
std::string ImportLsfgShaders(const std::string& dll, const std::string& output) {
    try {
        auto* parsed = peparse::ParsePEFromFile(dll.c_str());
        if (!parsed)
            return "Not a readable Lossless.dll PE file";
        Resources resources;
        peparse::IterRsrc(parsed, Resource, &resources);
        peparse::DestructParsedPE(parsed);
        // Validate/convert the whole set before writing any files.
        std::map<std::string, std::vector<uint8_t>> converted;
        for (const auto& [name, id] : shaders) {
            const auto it = resources.find(id);
            if (it == resources.end())
                return "Missing LSFG 3.1 FP32 resource: " + name;
            auto bytes = it->second;
            if (bytes.size() >= 4 && std::memcmp(bytes.data(), "DXBC", 4) == 0) {
                bytes = Extract::translateShader(std::move(bytes));
            }
            if (bytes.size() < 20 || bytes.size() % 4 || bytes[0] != 3 || bytes[1] != 2 ||
                bytes[2] != 35 || bytes[3] != 7) {
                return "Unsupported shader format: " + name;
            }
            // Reject truncated instruction streams before handing bytecode to a GPU driver.
            std::vector<uint32_t> words(bytes.size() / 4);
            std::memcpy(words.data(), bytes.data(), bytes.size());
            if (words[1] > 0x00010600 || words[3] == 0 || words[4] != 0)
                return "Unsupported SPIR-V header: " + name;
            bool compute_entry = false;
            for (std::size_t offset = 5; offset < words.size();) {
                const auto count = words[offset] >> 16;
                const auto opcode = words[offset] & 0xffff;
                if (!count || count > words.size() - offset)
                    return "Truncated SPIR-V instruction: " + name;
                if (opcode == 15 && count >= 4 && words[offset + 1] == 5)
                    compute_entry = true;
                offset += count;
            }
            if (!compute_entry)
                return "Not a compute shader: " + name;
            converted.emplace(name, std::move(bytes));
        }
        std::filesystem::create_directories(output);
        for (const auto& [name, bytes] : converted) {
            std::ofstream stream(output + "/" + name + ".spv", std::ios::binary);
            stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            if (!stream)
                return "Unable to save LSFG shaders";
        }
        return {};
    } catch (const std::exception& error) {
        return error.what();
    }
}
std::array<double, 4> GetLsfgStats() {
    return {static_cast<double>(real_presentations.load()),
            static_cast<double>(generated_presentations.load()), generation_ms.load(),
            static_cast<double>(status.load())};
}
void CountLsfgPresent(bool generated) {
    if (generated)
        ++generated_presentations;
    else
        ++real_presentations;
}
void SetLsfgStatus(int state, double milliseconds) {
    status = state;
    generation_ms = milliseconds;
}
} // namespace Vulkan
