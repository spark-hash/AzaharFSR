// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

import android.net.Uri
import androidx.preference.PreferenceManager
import java.io.File
import java.security.MessageDigest
import java.util.UUID
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary

object LsfgHelper {
    private const val KEY = "lsfg_shader_hash"
    private val context get() = CitraApplication.appContext

    fun activate() {
        val hash = PreferenceManager.getDefaultSharedPreferences(context).getString(KEY, "") ?: ""
        val directory = File(context.filesDir, "lsfg/$hash")
        val ready = Regex("[0-9a-f]{64}").matches(hash) && File(directory, "import.ok").isFile
        NativeLibrary.setLsfgShaderDirectory(if (ready) directory.absolutePath else "")
    }

    fun isReady(): Boolean {
        val hash = PreferenceManager.getDefaultSharedPreferences(context).getString(KEY, "") ?: ""
        return Regex("[0-9a-f]{64}").matches(hash) &&
            File(context.filesDir, "lsfg/$hash/import.ok").isFile
    }

    /** Called on Dispatchers.IO. The DLL is read as data and deleted after extracting shaders. */
    fun importDll(uri: Uri) {
        val dll = File.createTempFile("lossless-", ".dll", context.cacheDir)
        val staging = File(context.filesDir, "lsfg/import-${UUID.randomUUID()}")
        try {
            val digest = MessageDigest.getInstance("SHA-256")
            context.contentResolver.openInputStream(uri).use { input ->
                requireNotNull(input) { "Unable to read selected file" }
                dll.outputStream().use { output ->
                    val buffer = ByteArray(65536)
                    var total = 0L
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        require(total <= 64 * 1024 * 1024) { "Lossless.dll is larger than 64 MiB" }
                        digest.update(buffer, 0, count)
                        output.write(buffer, 0, count)
                    }
                }
            }
            val hash = digest.digest().joinToString("") { "%02x".format(it) }
            val destination = File(context.filesDir, "lsfg/$hash")
            if (!File(destination, "import.ok").isFile) {
                val error = NativeLibrary.importLsfgShaders(dll.absolutePath, staging.absolutePath)
                check(error.isEmpty()) { error }
                File(staging, "import.ok").writeText(hash)
                check(staging.renameTo(destination)) { "Unable to activate imported shaders" }
            }
            check(
                PreferenceManager.getDefaultSharedPreferences(
                    context
                ).edit().putString(KEY, hash).commit()
            )
            activate()
        } finally {
            dll.delete()
            staging.deleteRecursively()
        }
    }
}
