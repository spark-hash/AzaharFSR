// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.ByteArrayOutputStream
import java.io.File
import java.security.MessageDigest
import java.util.UUID
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary

object SharedFontHelper {
    data class Asset(val hash: String, val name: String)
    private val context get() = CitraApplication.appContext
    private val directory get() = File(context.filesDir, "shared_fonts")

    fun activate() = NativeLibrary.setSharedFontDirectory(directory.absolutePath)

    fun assets(): List<Asset> = directory.listFiles().orEmpty().filter {
        it.name.matches(Regex("[0-9a-f]{64}\\.bcfnt"))
    }.map {
        val hash = it.nameWithoutExtension
        Asset(
            hash,
            File(directory, "$hash.name").takeIf { name -> name.isFile }
                ?.readText()?.take(128) ?: hash.take(12)
        )
    }.sortedBy { it.name }

    /** Validate before committing an immutable local font file. Called on Dispatchers.IO. */
    fun importFont(uri: Uri): Asset {
        val bytes = context.contentResolver.openInputStream(uri).use { input ->
            requireNotNull(input) { "Unable to read selected font" }
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(65536)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                require(output.size() + count <= 0x332000) { "Font exceeds 3DS shared memory" }
                output.write(buffer, 0, count)
            }
            output.toByteArray()
        }
        val error = NativeLibrary.validateSharedFont(bytes)
        check(error.isEmpty()) { error }
        val hash = MessageDigest.getInstance("SHA-256").digest(bytes)
            .joinToString("") { "%02x".format(it) }
        val name = DocumentFile.fromSingleUri(context, uri)?.name?.take(128) ?: hash.take(12)
        check(directory.isDirectory || directory.mkdirs()) { "Unable to create font directory" }
        val target = File(directory, "$hash.bcfnt")
        if (!target.exists()) {
            val temporary = File(directory, "import-${UUID.randomUUID()}")
            try {
                temporary.writeBytes(bytes)
                check(temporary.renameTo(target) || target.isFile) {
                    "Unable to save imported font"
                }
            } finally {
                temporary.delete()
            }
        }
        File(directory, "$hash.name").writeText(name)
        activate()
        return Asset(hash, name)
    }
}
