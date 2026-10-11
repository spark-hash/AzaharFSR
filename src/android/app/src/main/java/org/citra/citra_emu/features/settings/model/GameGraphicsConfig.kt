// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.settings.model

import java.io.StringReader
import java.io.StringWriter
import org.ini4j.Wini

/** Sparse overrides. An absent key always follows the current global value. */
class GameGraphicsConfig(text: String = "") {
    private val ini = Wini(StringReader(text))

    init {
        if (getOverride("Renderer", "upscaling_mode") == null) {
            getOverride("Renderer", "fsr_enabled")?.let {
                ini.put(
                    "Renderer",
                    "upscaling_mode",
                    if (it == "1" ||
                        it.equals("true", true)
                    ) {
                        "1"
                    } else {
                        "0"
                    }
                )
            }
        }
    }

    fun getOverride(section: String, key: String): String? = ini[section]?.get(key)

    fun effective(
        section: String,
        key: String,
        global: GameGraphicsConfig,
        fallback: String
    ): String = getOverride(section, key) ?: global.getOverride(section, key) ?: fallback

    fun set(section: String, key: String, value: String?) {
        if (section == "Renderer" && key == "upscaling_mode") ini[section]?.remove("fsr_enabled")
        if (value == null) {
            ini[section]?.remove(key)
            if (ini[section]?.isEmpty() == true) ini.remove(section)
        } else {
            require(!value.contains('\n') && !value.contains('\r'))
            ini.put(section, key, value)
        }
    }

    fun clear() = ini.clear()

    fun encode(): String = StringWriter().also { ini.store(it) }.toString()
}
