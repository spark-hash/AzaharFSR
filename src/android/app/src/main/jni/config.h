// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/settings.h"

class INIReader;

class Config {
private:
    std::unique_ptr<INIReader> android_config;
    std::unique_ptr<INIReader> game_config;
    std::string game_config_text;
    std::string android_config_loc;

    bool LoadINI(const std::string& default_contents = "", bool retry = true);
    void ReadValues();
    INIReader& ReaderFor(const std::string& group, const std::string& key);

public:
    explicit Config(const std::string& game_id = {}, const std::string* game_ini = nullptr);
    ~Config();

    void Reload();
    const std::string& GameConfigText() const {
        return game_config_text;
    }

private:
    /**
     * Applies a value read from the android_config to a Setting.
     *
     * @param group The name of the INI group
     * @param setting The yuzu setting to modify
     */
    template <typename Type, bool ranged>
    void ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting);
};
