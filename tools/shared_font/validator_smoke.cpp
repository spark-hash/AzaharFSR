// SPDX-License-Identifier: GPL-2.0-or-later
// Build with font_validation.cpp; pass the generated uncompressed BCFNT
// fixture.
#include "core/hle/service/apt/bcfnt/font_validation.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Service::APT::BCFNT;
using Bytes = std::vector<std::uint8_t>;
static std::uint32_t Word(const Bytes &bytes, std::size_t offset) {
  return bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}
static void Set(Bytes &bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes[offset + i] = value >> (i * 8);
}
static void Check(bool pass, const char *name) {
  if (!pass)
    throw std::runtime_error(name);
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  std::ifstream input(argv[1], std::ios::binary);
  Bytes bytes((std::istreambuf_iterator<char>(input)), {});
  Check(!bytes.empty(), "fixture exists");
  auto normalized = NormalizeFont(bytes);
  Check(normalized.error.empty() && normalized.data == bytes,
        "ordinary BCFNT round trip");
  const auto tglp = Word(bytes, 36) - 8, cwdh = Word(bytes, 40) - 8,
             cmap = Word(bytes, 44) - 8;
  Bytes dump(0x80, 0);
  dump.insert(dump.end(), bytes.begin(), bytes.end());
  dump[0x83] = 'U';
  for (const auto offset : {36u, 40u, 44u, tglp + 28})
    Set(dump, 0x80 + offset, Word(bytes, offset) + 0x18000080);
  normalized = NormalizeFont(dump);
  Check(normalized.error.empty() && normalized.data == bytes,
        "relocated shared_font.bin round trip");
  auto bad = bytes;
  bad.resize(bad.size() - 1);
  Check(!NormalizeFont(bad).error.empty(), "truncated font rejected");
  bad = bytes;
  Set(bad, 36, 0xffffffff);
  Check(!NormalizeFont(bad).error.empty(), "invalid FINF pointer rejected");
  bad = bytes;
  Set(bad, tglp + 28, 0xffffffff);
  Check(!NormalizeFont(bad).error.empty(), "invalid texture pointer rejected");
  bad = bytes;
  Set(bad, cmap + 16, cmap + 8);
  Check(!NormalizeFont(bad).error.empty(), "cyclic CMAP rejected");
  bad = bytes;
  Set(bad, cwdh + 12, cwdh + 8);
  Check(!NormalizeFont(bad).error.empty(), "cyclic CWDH rejected");
  bad = bytes;
  Set(bad, cmap + 4, 0x7fffffff);
  Check(!NormalizeFont(bad).error.empty(), "oversized section rejected");
  bad = bytes;
  bad[cmap + 24] = 0xff;
  bad[cmap + 25] = 0xff;
  Check(!NormalizeFont(bad).error.empty(), "invalid glyph index rejected");
  bad = bytes;
  bad[15] = 0xff;
  Check(!NormalizeFont(bad).error.empty(), "forged size rejected");
  Check(!NormalizeFont(Bytes(SharedFontMemorySize + 1)).error.empty(),
        "memory overflow rejected");
  bad = bytes;
  Set(bad, 16, 4097);
  Check(!NormalizeFont(bad).error.empty(), "excessive section count rejected");
  bad = bytes;
  bad[cmap + 12] = 1;
  bad[cmap + 10] = bad[cmap + 8];
  bad[cmap + 11] = bad[cmap + 9];
  bad[cmap + 20] = 0xfe;
  bad[cmap + 21] = 0xff;
  Check(!NormalizeFont(bad).error.empty(), "invalid table glyph rejected");
  std::cout << "13 shared font validation checks passed\n";
}
