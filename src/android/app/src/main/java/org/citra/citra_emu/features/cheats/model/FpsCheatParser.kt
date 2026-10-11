// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.cheats.model

import java.util.Locale

data class FpsCheatDownload(val name: String, val code: String, val notes: String)

/** Converts supported CTRPF sections into Gateway codes, without executing downloaded data. */
object FpsCheatParser {
    fun titleIdHex(titleId: Long): String = String.format(Locale.ROOT, "%016X", titleId)

    fun matchesTitle(path: String, titleId: Long): Boolean = path.startsWith("Cheats/") &&
        path.substringAfterLast('/').equals("${titleIdHex(titleId)}.txt", true)

    fun normalizedCode(code: String): String = code.trim().uppercase(Locale.ROOT)
        .lines().joinToString("\n") { it.trim().replace(Regex("\\s+"), " ") }

    fun parse(text: String): List<FpsCheatDownload> {
        val results = mutableListOf<FpsCheatDownload>()
        val folders = mutableListOf<String>()
        var name = ""
        var qualifiedName = ""
        val code = mutableListOf<String>()
        val notes = mutableListOf<String>()
        var invalid = false
        var inComment = false
        fun flush() {
            // A 30 FPS section mentioning a hold-button 60 FPS mode is not a 60 FPS unlock.
            if (Regex("^(?:Default\\s+)?60\\s*(?:\\+\\+)?\\s*FPS\\b", RegexOption.IGNORE_CASE)
                    .containsMatchIn(name) && !invalid && !inComment && supported(code)
            ) {
                results +=
                    FpsCheatDownload(
                        qualifiedName,
                        code.joinToString("\n"),
                        notes.joinToString("\n")
                    )
            }
            code.clear()
            notes.clear()
            invalid = false
            inComment = false
        }
        for (raw in text.removePrefix("\uFEFF").lineSequence()) {
            val line = raw.trim()
            if (inComment || line.startsWith('{')) {
                notes += line.removePrefix("{").removeSuffix("}")
                inComment = !line.endsWith('}')
            } else if (line.startsWith('[') && line.endsWith(']')) {
                flush()
                name = line.substring(1, line.length - 1)
                when {
                    name == "--" -> if (folders.isNotEmpty()) folders.removeAt(folders.lastIndex)

                    name.startsWith("++") && name.endsWith("++") ->
                        folders +=
                            name.removeSurrounding("++")
                }
                qualifiedName = (folders + name).joinToString(" / ")
            } else if (line.startsWith('*') || line.startsWith("//")) {
                notes += line.removePrefix("*").removePrefix("//").trim()
            } else if (line.isNotEmpty()) {
                val instruction = line.substringBefore("//").trim()
                if (Regex("[0-9a-fA-F]{8}\\s+[0-9a-fA-F]{8}").matches(instruction)) {
                    code += normalizedCode(instruction)
                } else {
                    invalid = true // Never silently remove an instruction from a section.
                }
            }
        }
        flush()
        return results.distinctBy { it.code }
    }

    private fun supported(lines: List<String>): Boolean {
        if (lines.isEmpty() || lines.size > 1024) return false
        var index = 0
        while (index < lines.size) {
            val words = lines[index].split(' ')
            val first = words[0].toLong(16)
            val value = words[1].toLong(16)
            when (first ushr 28) {
                13L -> {
                    val subtype = (first ushr 24) and 15
                    if (subtype > 13) return false
                    // CTRPF extends standard D0/D1/D2 with non-zero operands; Gateway does not.
                    if (subtype <= 2 && ((first and 0xFFFFFFL) != 0L || value != 0L)) return false
                }

                14L -> {
                    val payloadLines = (value + 7) / 8
                    if (value == 0L || payloadLines > lines.size - index - 1) return false
                    index += payloadLines.toInt() // Embedded patch data is not an opcode.
                }

                15L -> return false
            }
            index++
        }
        return true
    }
}
