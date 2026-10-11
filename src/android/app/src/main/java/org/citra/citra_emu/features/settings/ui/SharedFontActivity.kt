// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.settings.ui

import android.os.Bundle
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.citra.citra_emu.R
import org.citra.citra_emu.features.settings.SettingKeys
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.features.settings.model.StringSetting
import org.citra.citra_emu.features.settings.utils.GameSettingsStore
import org.citra.citra_emu.utils.SharedFontHelper
import org.citra.citra_emu.utils.ThemeUtil

class SharedFontActivity : AppCompatActivity() {
    private val gameId get() = intent.getStringExtra("game_settings_id")
    private lateinit var status: TextView
    private lateinit var importButton: MaterialButton
    private lateinit var chooseButton: MaterialButton
    private val pickFont =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                lifecycleScope.launch {
                    busy(true)
                    try {
                        val asset = withContext(Dispatchers.IO) { SharedFontHelper.importFont(uri) }
                        select(asset.hash)
                    } catch (cancelled: CancellationException) {
                        throw cancelled
                    } catch (error: Exception) {
                        MaterialAlertDialogBuilder(this@SharedFontActivity)
                            .setMessage(getString(R.string.shared_font_error, error.message))
                            .setPositiveButton(android.R.string.ok, null).show()
                    } finally {
                        busy(false)
                        refresh()
                    }
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeUtil.setTheme(this)
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(
                WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout()
            )
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
        root.addView(
            MaterialToolbar(this).apply {
                setTitle(R.string.shared_font_assets)
                setNavigationIcon(R.drawable.ic_back)
                setNavigationOnClickListener { finish() }
            }
        )
        val rows = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val padding = (16 * resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
        }
        rows.addView(TextView(this).apply { setText(R.string.shared_font_import_description) })
        status = TextView(this)
        rows.addView(status)
        importButton = MaterialButton(this).apply {
            setText(R.string.shared_font_import)
            setOnClickListener { pickFont.launch(arrayOf("*/*")) }
        }
        chooseButton = MaterialButton(this).apply {
            setText(R.string.shared_font_choose)
            setOnClickListener {
                val assets = SharedFontHelper.assets()
                val names =
                    listOf(
                        getString(
                            if (gameId ==
                                null
                            ) {
                                R.string.shared_font_clear
                            } else {
                                R.string.shared_font_inherit
                            }
                        )
                    ) +
                        assets.map { it.name }
                MaterialAlertDialogBuilder(this@SharedFontActivity)
                    .setItems(names.toTypedArray()) { _, which ->
                        lifecycleScope.launch {
                            try {
                                select(if (which == 0) null else assets[which - 1].hash)
                            } catch (
                                error: Exception
                            ) {
                                MaterialAlertDialogBuilder(this@SharedFontActivity)
                                    .setMessage(
                                        getString(R.string.shared_font_error, error.message)
                                    )
                                    .setPositiveButton(android.R.string.ok, null).show()
                            }
                            refresh()
                        }
                    }.show()
            }
        }
        rows.addView(importButton)
        rows.addView(chooseButton)
        root.addView(ScrollView(this).apply { addView(rows) }, LinearLayout.LayoutParams(-1, 0, 1f))
        setContentView(root)
        refresh()
    }

    private suspend fun select(hash: String?) = withContext(Dispatchers.IO) {
        val id = gameId
        val config = if (id == null) GameSettingsStore.global() else GameSettingsStore.read(id)
        config.set(Settings.SECTION_UTILITY, SettingKeys.shared_font_file(), hash)
        config.set(
            Settings.SECTION_UTILITY,
            SettingKeys.shared_font_mode(),
            if (hash ==
                null
            ) {
                null
            } else {
                "2"
            }
        )
        if (id == null) {
            GameSettingsStore.saveGlobal(config)
            StringSetting.SHARED_FONT_FILE.string = hash ?: ""
            IntSetting.SHARED_FONT_MODE.int = if (hash == null) 0 else 2
        } else {
            GameSettingsStore.save(id, config)
        }
    }

    private fun busy(value: Boolean) {
        importButton.isEnabled = !value
        chooseButton.isEnabled = !value
    }

    private fun refresh() {
        try {
            val global = GameSettingsStore.global()
            val config = gameId?.let { GameSettingsStore.read(it) } ?: global
            val hash = config.effective(
                Settings.SECTION_UTILITY,
                SettingKeys.shared_font_file(),
                global,
                ""
            )
            val mode =
                config.effective(
                    Settings.SECTION_UTILITY,
                    SettingKeys.shared_font_mode(),
                    global,
                    "0"
                ).toIntOrNull()
                    ?: 0
            val label = SharedFontHelper.assets().firstOrNull { it.hash == hash }?.name
                ?: getString(R.string.shared_font_none)
            status.text =
                resources.getStringArray(R.array.sharedFontNames)[mode.coerceIn(0, 3)] + "\n" +
                label
        } catch (error: Exception) {
            status.text = getString(R.string.shared_font_error, error.message)
        }
    }
}
