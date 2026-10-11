// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include "common/file_derived.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/romfs_reader.h"
#include "core/file_sys/seed_db.h"
#include "core/hw/aes/key.h"
#include "core/loader/loader.h"

namespace {
using Key = std::array<u8, 16>;
using Status = Loader::ResultStatus;
std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
struct Fixture {
    std::filesystem::path dir =
        std::filesystem::current_path() /
        ("ncch-crypto-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string previous = FileUtil::GetUserPath(FileUtil::UserPath::UserDir);
    Fixture() {
        std::filesystem::create_directories(dir / "sysdata");
        std::ofstream(dir / "sysdata/aes_keys.txt")
            << ":AES\ngeneratorConstant=00000000000000000000000000000000\n";
        FileUtil::SetUserPath(Utf8(dir));
        HW::AES::InitKeys(true);
    }
    ~Fixture() {
        FileUtil::SetUserPath(previous);
        std::error_code error;
        std::filesystem::remove_all(dir, error);
    }
};
struct SyntheticNCCH {
    NCCH_Header header{};
    std::vector<u8> bytes;
    std::vector<u8> code = std::vector<u8>(73, 0x53);
    std::vector<u8> icon = std::vector<u8>(31, 0x19);
    std::vector<u8> romfs;
    Key primary{}, secondary{};
    SyntheticNCCH(u16 version, bool fixed = true, u8 selector = 0, bool seeded = false) {
        header.magic = FileUtil::MakeMagic('N', 'C', 'C', 'H');
        header.version = version;
        header.content_size = 0x100;
        header.program_id = 0x000400000FF12300ULL;
        header.fixed_key.Assign(fixed);
        header.extended_header_size = 0x400;
        header.exefs_offset = 5;
        header.exefs_size = 2;
        header.exefs_hash_region_size = 1;
        header.romfs_offset = 7;
        header.romfs_size = 0x20;
        for (u32 i = 0; i < 8; ++i)
            header.partition_id[i] = i + 3;
        if (!fixed) {
            Key x{}, y{};
            x.fill(0x35);
            y.fill(0x72);
            std::copy(y.begin(), y.end(), header.signature);
            HW::AES::SetKeyX(HW::AES::KeySlotID::NCCHSecure1, x);
            HW::AES::SetKeyY(HW::AES::KeySlotID::NCCHSecure1, y);
            primary = HW::AES::GetNormalKey(HW::AES::KeySlotID::NCCHSecure1);
            header.secondary_key_slot = selector;
            header.seed_crypto.Assign(seeded);
            if (seeded) {
                Key seed{};
                seed.fill(0x91);
                std::array<u8, 24> check{};
                std::copy(seed.begin(), seed.end(), check.begin());
                std::memcpy(check.data() + 16, &header.program_id, 8);
                std::array<u8, 32> hash{};
                CryptoPP::SHA256{}.CalculateDigest(hash.data(), check.data(), check.size());
                std::copy_n(hash.begin(), 4, header.reserved_0);
                std::array<u8, 32> input{};
                std::copy(y.begin(), y.end(), input.begin());
                std::copy(seed.begin(), seed.end(), input.begin() + 16);
                CryptoPP::SHA256{}.CalculateDigest(hash.data(), input.data(), input.size());
                std::copy_n(hash.begin(), 16, y.begin());
            }
            const std::size_t slot = selector == 1    ? HW::AES::NCCHSecure2
                                     : selector == 10 ? HW::AES::NCCHSecure3
                                     : selector == 11 ? HW::AES::NCCHSecure4
                                                      : HW::AES::NCCHSecure1;
            HW::AES::SetKeyX(slot, x);
            HW::AES::SetKeyY(slot, y);
            secondary = HW::AES::GetNormalKey(slot);
        }
        ExHeader_Header exheader{};
        exheader.system_info.jump_id = header.program_id;
        exheader.arm11_system_local_caps.program_id = header.program_id;
        CryptoPP::SHA256{}.CalculateDigest(header.extended_header_hash,
                                           reinterpret_cast<const u8*>(&exheader), 0x400);
        ExeFs_Header exefs{};
        std::memcpy(exefs.section[0].name, ".code", 5);
        exefs.section[0].size = code.size();
        std::memcpy(exefs.section[1].name, "icon", 4);
        exefs.section[1].offset = 0x80;
        exefs.section[1].size = icon.size();
        CryptoPP::SHA256{}.CalculateDigest(exefs.hashes[7], code.data(), code.size());
        CryptoPP::SHA256{}.CalculateDigest(exefs.hashes[6], icon.data(), icon.size());
        CryptoPP::SHA256{}.CalculateDigest(header.exefs_super_block_hash,
                                           reinterpret_cast<const u8*>(&exefs), sizeof(exefs));
        bytes.resize(0x4e00);
        std::memcpy(bytes.data(), &header, sizeof(header));
        std::memcpy(bytes.data() + 0x200, &exheader, sizeof(exheader));
        std::memcpy(bytes.data() + 0xa00, &exefs, sizeof(exefs));
        std::copy(code.begin(), code.end(), bytes.begin() + 0xc00);
        std::copy(icon.begin(), icon.end(), bytes.begin() + 0xc80);
        romfs.resize(0x3000);
        for (u32 i = 0; i < romfs.size(); ++i)
            romfs[i] = (i * 17 + 29) % 251;
        std::copy(romfs.begin(), romfs.end(), bytes.begin() + 0x1e00);
        Encrypt(1, 0x200, sizeof(exheader), 0, primary);
        Encrypt(2, 0xa00, sizeof(exefs), 0, primary);
        Encrypt(2, 0xc00, code.size(), 0x200, secondary);
        Encrypt(2, 0xc80, icon.size(), 0x280, primary);
        Encrypt(3, 0x1e00, romfs.size(), 0x1000, secondary);
    }
    Key Counter(u32 section) const {
        Key ctr{};
        if (header.version == 1) {
            std::copy_n(header.partition_id, 8, ctr.begin());
            const u32 base = section == 1 ? 0x200 : section == 2 ? 0xa00 : 0xe00;
            for (u32 i = 0; i < 4; ++i)
                ctr[12 + i] = base >> (24 - i * 8);
        } else {
            std::reverse_copy(header.partition_id, header.partition_id + 8, ctr.begin());
            ctr[8] = section;
        }
        return ctr;
    }
    void Encrypt(u32 section, u32 offset, u32 size, u32 seek, const Key& key) {
        const auto ctr = Counter(section);
        CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc(key.data(), key.size(), ctr.data());
        enc.Seek(seek);
        enc.ProcessData(bytes.data() + offset, bytes.data() + offset, size);
    }
    std::string Save(const Fixture& fixture) const {
        const auto path = fixture.dir / "synthetic.cci";
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return Utf8(path);
    }
};
} // namespace

TEST_CASE("NCCH encrypted sections and random RomFS reads", "[ncch-crypto]") {
    Fixture fixture;
    for (u16 version : {0, 1, 2}) {
        SyntheticNCCH image(version);
        FileSys::NCCHContainer container(image.Save(fixture));
        REQUIRE(container.Load() == Status::Success);
        std::vector<u8> result;
        REQUIRE(container.LoadSectionExeFS(".code", result) == Status::Success);
        REQUIRE(result == image.code);
        REQUIRE(container.LoadSectionExeFS("icon", result) == Status::Success);
        REQUIRE(result == image.icon);
        std::shared_ptr<FileSys::RomFSReader> romfs;
        REQUIRE(container.ReadRomFS(romfs, false) == Status::Success);
        for (auto [offset, length] : {std::pair{7u, 37u}, std::pair{8190u, 25u},
                                      std::pair{3u, 9001u}, std::pair{7u, 37u}}) {
            result.resize(length);
            REQUIRE(romfs->ReadFile(offset, length, result.data()) == length);
            REQUIRE(std::equal(result.begin(), result.end(), image.romfs.begin() + offset));
        }
        result.resize(32);
        REQUIRE(romfs->ReadFile(image.romfs.size(), 32, result.data()) == 0);
        REQUIRE(romfs->ReadFile(image.romfs.size() - 7, 32, result.data()) == 7);
    }
}

TEST_CASE("NCCH retail key derivation and wrong-key rejection", "[ncch-crypto]") {
    Fixture fixture;
    SyntheticNCCH image(2, false);
    const auto path = image.Save(fixture);
    FileSys::NCCHContainer good(path);
    REQUIRE(good.Load() == Status::Success);
    Key wrong{};
    wrong.fill(0xc7);
    HW::AES::SetKeyX(HW::AES::KeySlotID::NCCHSecure1, wrong);
    FileSys::NCCHContainer bad(path);
    REQUIRE(bad.Load() == Status::ErrorEncrypted);
}

TEST_CASE("Encrypted NCCH rejects corrupt header and keeps plaintext overrides", "[ncch-crypto]") {
    Fixture fixture;
    SyntheticNCCH image(2);
    auto path = image.Save(fixture);
    auto exefs = image.bytes;
    const auto ctr = image.Counter(2);
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec(image.primary.data(), 16, ctr.data());
    dec.ProcessData(exefs.data() + 0xa00, exefs.data() + 0xa00, sizeof(ExeFs_Header));
    std::copy(image.code.begin(), image.code.end(), exefs.begin() + 0xc00);
    {
        std::ofstream file(path + ".exefs", std::ios::binary);
        file.write(reinterpret_cast<const char*>(exefs.data() + 0xa00), 0x400);
    }
    FileSys::NCCHContainer modded(path);
    std::vector<u8> result;
    REQUIRE(modded.LoadSectionExeFS(".code", result) == Status::Success);
    REQUIRE(result == image.code);
    image.bytes[0x223] ^= 0x41;
    path = image.Save(fixture);
    FileSys::NCCHContainer bad(path);
    REQUIRE(bad.Load() == Status::ErrorEncrypted);
}

TEST_CASE("NCCH secondary selectors and seed verification", "[ncch-crypto]") {
    Fixture fixture;
    for (u8 selector : {0, 1, 10, 11}) {
        for (bool seeded : {false, true}) {
            SyntheticNCCH image(2, false, selector, seeded);
            const auto path = image.Save(fixture);
            if (seeded) {
                FileSys::DeleteSeed(image.header.program_id);
                FileSys::NCCHContainer missing(path);
                REQUIRE(missing.Load() == Status::ErrorEncrypted);
                FileSys::Seed seed{};
                seed.title_id = image.header.program_id;
                seed.data.fill(0x92);
                REQUIRE(FileSys::AddSeed(seed));
                FileSys::NCCHContainer wrong(path);
                REQUIRE(wrong.Load() == Status::ErrorEncrypted);
                REQUIRE(FileSys::DeleteSeed(seed.title_id));
                seed.data.fill(0x91);
                REQUIRE(FileSys::AddSeed(seed));
            }
            FileSys::NCCHContainer good(path);
            REQUIRE(good.Load() == Status::Success);
            std::vector<u8> result;
            REQUIRE(good.LoadSectionExeFS(".code", result) == Status::Success);
            REQUIRE(result == image.code);
            REQUIRE(good.LoadSectionExeFS("icon", result) == Status::Success);
            REQUIRE(result == image.icon);
            std::shared_ptr<FileSys::RomFSReader> reader;
            REQUIRE(good.ReadRomFS(reader, false) == Status::Success);
            result.resize(100);
            REQUIRE(reader->ReadFile(19, result.size(), result.data()) == result.size());
            REQUIRE(std::equal(result.begin(), result.end(), image.romfs.begin() + 19));
        }
    }
}

TEST_CASE("NCCH corrupt encrypted code is rejected before execution", "[ncch-crypto]") {
    Fixture fixture;
    SyntheticNCCH image(2);
    image.bytes[0xc13] ^= 1;
    FileSys::NCCHContainer container(image.Save(fixture));
    std::vector<u8> result;
    REQUIRE(container.LoadSectionExeFS(".code", result) == Status::ErrorEncrypted);
}

TEST_CASE("Encrypted RomFS superblock detects wrong secondary key", "[ncch-crypto]") {
    Fixture fixture;
    SyntheticNCCH image(2, false, 10);
    image.header.romfs_hash_region_size = 1;
    std::fill_n(image.bytes.begin() + 0xe00, 0x200, 0x71);
    CryptoPP::SHA256{}.CalculateDigest(image.header.romfs_super_block_hash,
                                       image.bytes.data() + 0xe00, 0x200);
    image.Encrypt(3, 0xe00, 0x200, 0, image.secondary);
    std::memcpy(image.bytes.data(), &image.header, sizeof(image.header));
    const auto path = image.Save(fixture);
    FileSys::NCCHContainer good(path);
    REQUIRE(good.Load() == Status::Success);
    Key wrong{};
    wrong.fill(0x13);
    HW::AES::SetKeyX(HW::AES::NCCHSecure3, wrong);
    FileSys::NCCHContainer bad(path);
    REQUIRE(bad.Load() == Status::ErrorEncrypted);
}

TEST_CASE("NCSD cartridges and plaintext NCCH still load", "[ncch-crypto]") {
    Fixture fixture;
    for (bool plaintext : {false, true}) {
        SyntheticNCCH image(2);
        if (plaintext) {
            image.Encrypt(1, 0x200, sizeof(ExHeader_Header), 0, image.primary);
            image.Encrypt(2, 0xa00, sizeof(ExeFs_Header), 0, image.primary);
            image.Encrypt(2, 0xc00, image.code.size(), 0x200, image.secondary);
            image.Encrypt(2, 0xc80, image.icon.size(), 0x280, image.primary);
            image.Encrypt(3, 0x1e00, image.romfs.size(), 0x1000, image.secondary);
            image.header.no_crypto.Assign(1);
            std::memcpy(image.bytes.data(), &image.header, sizeof(image.header));
        }
        NCSD_Header cartridge{};
        cartridge.magic = FileUtil::MakeMagic('N', 'C', 'S', 'D');
        cartridge.partitions[0].offset = 2;
        cartridge.partitions[0].size = image.bytes.size() / 0x200;
        std::vector<u8> wrapped(image.bytes.size() + 0x400);
        std::memcpy(wrapped.data(), &cartridge, sizeof(cartridge));
        std::copy(image.bytes.begin(), image.bytes.end(), wrapped.begin() + 0x400);
        image.bytes = std::move(wrapped);
        FileSys::NCCHContainer container(image.Save(fixture));
        std::vector<u8> result;
        REQUIRE(container.LoadSectionExeFS(".code", result) == Status::Success);
        REQUIRE(result == image.code);
        std::shared_ptr<FileSys::RomFSReader> reader;
        REQUIRE(container.ReadRomFS(reader, false) == Status::Success);
        result.resize(64);
        REQUIRE(reader->ReadFile(9, result.size(), result.data()) == result.size());
        REQUIRE(std::equal(result.begin(), result.end(), image.romfs.begin() + 9));
    }
}

TEST_CASE("Wrong secondary key fails code verification while icon remains readable",
          "[ncch-crypto]") {
    Fixture fixture;
    SyntheticNCCH image(2, false, 1);
    Key wrong{};
    wrong.fill(0xbc);
    HW::AES::SetKeyX(HW::AES::NCCHSecure2, wrong);
    FileSys::NCCHContainer container(image.Save(fixture));
    REQUIRE(container.Load() == Status::Success);
    std::vector<u8> result;
    REQUIRE(container.LoadSectionExeFS("icon", result) == Status::Success);
    REQUIRE(result == image.icon);
    REQUIRE(container.LoadSectionExeFS(".code", result) == Status::ErrorEncrypted);
}
