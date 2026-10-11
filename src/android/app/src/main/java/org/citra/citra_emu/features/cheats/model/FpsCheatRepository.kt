// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.cheats.model

import java.io.ByteArrayOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import org.json.JSONObject

object FpsCheatRepository {
    const val SOURCE = "https://github.com/Reshiban/60FPS-AR-CHEATS-3DS"
    private const val API = "https://api.github.com/repos/Reshiban/60FPS-AR-CHEATS-3DS"

    /** Run on Dispatchers.IO. Pin index and files to the same upstream commit. */
    fun download(titleId: Long): List<FpsCheatDownload> {
        val revision = JSONObject(fetch("$API/commits/master", 262144)).getString("sha")
        check(Regex("[0-9a-f]{40}").matches(revision))
        val tree = JSONObject(fetch("$API/git/trees/$revision?recursive=1", 2097152))
        if (tree.optBoolean("truncated")) throw IOException("Incomplete database index")
        val entries = tree.getJSONArray("tree")
        val results = mutableListOf<FpsCheatDownload>()
        for (index in 0 until entries.length()) {
            val entry = entries.getJSONObject(index)
            val path = entry.getString("path")
            if (entry.getString("type") != "blob" ||
                !FpsCheatParser.matchesTitle(path, titleId)
            ) {
                continue
            }
            val encoded = path.split('/').joinToString("/") {
                URLEncoder.encode(it, "UTF-8").replace("+", "%20")
            }
            val rawSource = "https://raw.githubusercontent.com/Reshiban/60FPS-AR-CHEATS-3DS"
            val url = "$rawSource/$revision/$encoded"
            results += FpsCheatParser.parse(fetch(url, 131072)).map {
                it.copy(notes = "${it.notes}\n$SOURCE\n$path\nRevision: $revision".trim())
            }
        }
        return results.distinctBy { it.code }
    }

    private fun fetch(url: String, maxBytes: Int): String {
        val connection = URL(url).openConnection() as HttpURLConnection
        try {
            connection.connectTimeout = 10000
            connection.readTimeout = 15000
            connection.instanceFollowRedirects = false
            connection.setRequestProperty("User-Agent", "AzaharFSR-FPS-Cheats")
            connection.setRequestProperty("Accept", "application/vnd.github+json")
            val status = connection.responseCode
            if (status != 200) throw IOException("HTTP $status")
            return connection.inputStream.use {
                val output = ByteArrayOutputStream()
                val buffer = ByteArray(8192)
                while (true) {
                    val count = it.read(buffer)
                    if (count < 0) break
                    if (output.size() + count >
                        maxBytes
                    ) {
                        throw IOException("Database response too large")
                    }
                    output.write(buffer, 0, count)
                }
                output.toString("UTF-8")
            }
        } finally {
            connection.disconnect()
        }
    }
}
