// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.settings.utils

import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.security.MessageDigest
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.features.settings.model.GameGraphicsConfig
import org.citra.citra_emu.model.Game
import org.citra.citra_emu.utils.DirectoryInitialization

object GameSettingsStore {
    fun id(game: Game): String = if (game.titleId != 0L) {
        String.format(java.util.Locale.ROOT, "%016X", game.titleId)
    } else {
        "H" + MessageDigest.getInstance("SHA-256").digest(game.path.toByteArray())
            .joinToString("") { "%02X".format(it) }
    }

    private fun file(id: String, create: Boolean): DocumentFile? {
        require(id.matches(Regex("[A-Z0-9]{16,65}")))
        val root = DocumentFile.fromTreeUri(
            CitraApplication.appContext,
            Uri.parse(DirectoryInitialization.userDirectory)
        ) ?: error("User folder unavailable")
        val config = root.findFile("config") ?: if (create) root.createDirectory("config") else null
        val games = config?.findFile("android_games")
            ?: if (create) config?.createDirectory("android_games") else null
        return games?.findFile("$id.ini")
            ?: if (create) games?.createFile("application/octet-stream", "$id.ini") else null
    }

    fun read(id: String): GameGraphicsConfig {
        val file = file(id, false) ?: return GameGraphicsConfig()
        return CitraApplication.appContext.contentResolver.openInputStream(file.uri)!!.use {
            GameGraphicsConfig(it.bufferedReader().readText())
        }
    }

    fun global(): GameGraphicsConfig = CitraApplication.appContext.contentResolver
        .openInputStream(SettingsFile.getSettingsFile(SettingsFile.FILE_NAME_CONFIG).uri)!!.use {
        GameGraphicsConfig(it.bufferedReader().readText())
    }

    fun saveGlobal(config: GameGraphicsConfig) {
        val file = SettingsFile.getSettingsFile(SettingsFile.FILE_NAME_CONFIG)
        CitraApplication.appContext.contentResolver.openOutputStream(file.uri, "wt")!!.use {
            it.write(config.encode().toByteArray(Charsets.UTF_8))
            it.flush()
        }
    }

    fun save(id: String, config: GameGraphicsConfig) {
        val file = file(id, true) ?: error("Game settings file unavailable")
        CitraApplication.appContext.contentResolver.openOutputStream(file.uri, "wt")!!.use {
            it.write(config.encode().toByteArray(Charsets.UTF_8))
            it.flush()
        }
    }
}
