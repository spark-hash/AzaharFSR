// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

/** Format the running driver's strings, rather than the selected archive's metadata. */
object DriverSummary {
    fun family(name: String, version: String): String = when {
        (name + " " + version).contains("turnip", true) -> "turnip"
        name.contains("qualcomm", true) || name.contains("adreno", true) -> "qualcomm"
        else -> name.trim().substringBefore(' ').take(16)
    }

    fun version(backend: String, value: String): String {
        val vendorVersion = Regex("V@([0-9]+(?:\\.[0-9]+)+)", RegexOption.IGNORE_CASE)
            .find(value)?.groupValues?.get(1)
            ?: Regex("Mesa\\s+([0-9]+(?:\\.[0-9]+)+(?:-devel)?)", RegexOption.IGNORE_CASE)
                .find(value)?.groupValues?.get(1)
        // The first number in a GL_VERSION string is the API version, not the driver version.
        val numericVersion = if (backend == "Vulkan") {
            Regex("^\\s*([0-9]+(?:\\.[0-9]+){1,3}(?:-devel)?)")
                .find(value)?.groupValues?.get(1)
        } else {
            null
        }
        return (vendorVersion ?: numericVersion ?: "").take(24)
    }
}
