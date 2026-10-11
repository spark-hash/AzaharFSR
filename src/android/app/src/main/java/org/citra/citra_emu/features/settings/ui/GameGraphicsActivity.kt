// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.settings.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.InputType
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.citra.citra_emu.R
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.BooleanSetting
import org.citra.citra_emu.features.settings.model.FloatSetting
import org.citra.citra_emu.features.settings.model.GameGraphicsConfig
import org.citra.citra_emu.features.settings.model.IntSetting
import org.citra.citra_emu.features.settings.utils.GameSettingsStore
import org.citra.citra_emu.model.Game
import org.citra.citra_emu.utils.GpuDriverHelper
import org.citra.citra_emu.utils.GpuDriverMetadata
import org.citra.citra_emu.utils.ThemeUtil

/** Edits an independent document; never mutates the global setting enums or installed driver. */
class GameGraphicsActivity : AppCompatActivity() {
    private lateinit var id: String
    private lateinit var config: GameGraphicsConfig
    private lateinit var global: GameGraphicsConfig
    private lateinit var rows: LinearLayout
    private val refreshers = mutableListOf<() -> Unit>()
    private var drivers = emptyList<Pair<Uri, GpuDriverMetadata>>()

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeUtil.setTheme(this)
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        id = intent.getStringExtra("game_settings_id") ?: run {
            finish()
            return
        }
        try {
            config = GameSettingsStore.read(id)
            global = GameSettingsStore.global()
        } catch (e: Exception) {
            Toast.makeText(
                this,
                getString(R.string.game_settings_error, e.message),
                Toast.LENGTH_LONG
            ).show()
            finish()
            return
        }
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(
                WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout()
            )
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
        val toolbar = MaterialToolbar(this).apply {
            title = getString(R.string.game_graphics_settings)
            subtitle = intent.getStringExtra("game_title")
            setNavigationIcon(R.drawable.ic_back)
            setNavigationOnClickListener { finish() }
        }
        root.addView(toolbar)
        val scroll = ScrollView(this)
        rows = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val padding = (16 * resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
        }
        scroll.addView(rows)
        root.addView(scroll, LinearLayout.LayoutParams(-1, 0, 1f))
        setContentView(root)
        rows.addView(TextView(this).apply { text = getString(R.string.game_settings_hint) })
        row(getString(R.string.game_settings_reset), { "" }) {
            MaterialAlertDialogBuilder(this).setMessage(R.string.game_settings_reset_confirm)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    persist { config.clear() }
                }.show()
        }
        choice(
            IntSetting.SHARED_FONT_MODE,
            R.string.shared_font_mode,
            R.array.sharedFontNames,
            R.array.sharedFontValues
        )
        row(getString(R.string.shared_font_assets), {
            getString(R.string.shared_font_assets_description)
        }) {
            startActivity(
                Intent(this, SharedFontActivity::class.java).putExtra("game_settings_id", id)
            )
        }
        driverRow()
        choice(
            IntSetting.GRAPHICS_API,
            R.string.graphics_api,
            R.array.graphicsApiNames,
            R.array.graphicsApiValues
        )
        choice(
            IntSetting.RESOLUTION_FACTOR,
            R.string.internal_resolution,
            R.array.resolutionFactorNames,
            R.array.resolutionFactorValues
        )
        choice(
            IntSetting.ANTI_ALIASING,
            R.string.anti_aliasing,
            R.array.antiAliasingNames,
            R.array.antiAliasingValues
        )
        choice(
            IntSetting.UPSCALING_MODE,
            R.string.upscaling_mode,
            R.array.upscalingNames,
            R.array.upscalingValues
        )
        choice(IntSetting.LSFG_MODE, R.string.lsfg_mode, R.array.lsfgNames, R.array.lsfgValues)
        row(getString(R.string.lsfg_assets), { getString(R.string.lsfg_assets_description) }) {
            startActivity(Intent(this, LsfgActivity::class.java))
        }
        number(IntSetting.FSR_SHARPNESS, R.string.upscaling_sharpness, 0.0, 100.0)
        toggle(BooleanSetting.LINEAR_FILTERING, R.string.linear_filtering)
        toggle(BooleanSetting.USE_INTEGER_SCALING, R.string.use_integer_scaling)
        choice(
            IntSetting.TEXTURE_FILTER,
            R.string.texture_filter_name,
            R.array.textureFilterNames,
            R.array.textureFilterValues
        )
        choice(
            IntSetting.TEXTURE_SAMPLING,
            R.string.texture_sampling_name,
            R.array.textureSamplingNames,
            R.array.textureSamplingValues
        )
        toggle(BooleanSetting.USE_FRAME_LIMIT, R.string.frame_limit_enable)
        number(IntSetting.FRAME_LIMIT, R.string.frame_limit_slider, 1.0, 1000.0)
        toggle(BooleanSetting.VSYNC, R.string.vsync)
        toggle(BooleanSetting.USE_SKIP_DUPLICATE_FRAMES, R.string.game_skip_duplicate_frames)
        toggle(BooleanSetting.SPIRV_SHADER_GEN, R.string.spirv_shader_gen)
        toggle(BooleanSetting.DISABLE_SPIRV_OPTIMIZER, R.string.disable_spirv_optimizer)
        toggle(BooleanSetting.ASYNC_SHADERS, R.string.async_shaders)
        toggle(BooleanSetting.ASYNC_PRESENTATION, R.string.game_async_presentation)
        toggle(BooleanSetting.DISK_SHADER_CACHE, R.string.use_disk_shader_cache)
        toggle(BooleanSetting.SHADERS_ACCURATE_MUL, R.string.shaders_accurate_mul)
        toggle(BooleanSetting.HW_SHADER, R.string.hw_shaders)
        toggle(BooleanSetting.SHADER_JIT, R.string.shader_jit)
        toggle(BooleanSetting.SIMULATE_3DS_GPU_TIMINGS, R.string.simulate_3ds_gpu_timings)
        number(IntSetting.DELAY_RENDER_THREAD_US, R.string.delay_render_thread, 0.0, 16000.0)
        choice(
            IntSetting.RENDER_3D_WHICH_DISPLAY,
            R.string.render_3d_which_display,
            R.array.render3dWhichDisplay,
            R.array.render3dDisplayValues
        )
        choice(
            IntSetting.STEREOSCOPIC_3D_MODE,
            R.string.render3d,
            R.array.render3dModes,
            R.array.render3dValues
        )
        number(IntSetting.STEREOSCOPIC_3D_DEPTH, R.string.factor3d, 0.0, 255.0)
        toggle(BooleanSetting.DISABLE_RIGHT_EYE_RENDER, R.string.disable_right_eye_render)
        toggle(BooleanSetting.SWAP_EYES_3D, R.string.swap_eyes_3d)
        number(IntSetting.CARDBOARD_SCREEN_SIZE, R.string.cardboard_screen_size, 30.0, 100.0)
        number(IntSetting.CARDBOARD_X_SHIFT, R.string.cardboard_x_shift, -100.0, 100.0)
        number(IntSetting.CARDBOARD_Y_SHIFT, R.string.cardboard_y_shift, -100.0, 100.0)
        number(FloatSetting.SECOND_SCREEN_OPACITY, R.string.game_second_screen_opacity, 0.0, 100.0)
        number(FloatSetting.BACKGROUND_RED, R.string.game_background_red, 0.0, 1.0)
        number(FloatSetting.BACKGROUND_GREEN, R.string.game_background_green, 0.0, 1.0)
        number(FloatSetting.BACKGROUND_BLUE, R.string.game_background_blue, 0.0, 1.0)
        toggle(BooleanSetting.CUSTOM_TEXTURES, R.string.custom_textures)
        toggle(BooleanSetting.PRELOAD_TEXTURES, R.string.preload_textures)
        toggle(BooleanSetting.ASYNC_CUSTOM_LOADING, R.string.async_custom_loading)
        toggle(BooleanSetting.DUMP_TEXTURES, R.string.dump_textures)
        toggle(BooleanSetting.DEBUG_RENDERER, R.string.renderer_debug)
        lifecycleScope.launch {
            drivers = withContext(Dispatchers.IO) {
                runCatching { GpuDriverHelper.getDrivers() }.getOrDefault(emptyList())
            }
            refreshers.forEach { it() }
        }
    }

    override fun onResume() {
        super.onResume()
        if (::id.isInitialized && ::config.isInitialized) {
            runCatching {
                config = GameSettingsStore.read(id)
                global = GameSettingsStore.global()
                refreshers.forEach { it() }
            }
        }
    }

    private fun effective(setting: AbstractSetting, fallback: String) =
        config.effective(setting.section!!, setting.key!!, global, fallback)

    private fun summary(section: String, key: String, value: String): String =
        if (config.getOverride(section, key) ==
            null
        ) {
            getString(R.string.game_setting_inherited, value)
        } else {
            getString(R.string.game_setting_override, value)
        }

    private fun row(title: String, value: () -> String, action: () -> Unit) {
        val button = MaterialButton(
            this,
            null,
            com.google.android.material.R.attr.materialButtonOutlinedStyle
        ).apply {
            gravity = Gravity.START or Gravity.CENTER_VERTICAL
            isAllCaps = false
            setOnClickListener { action() }
        }
        val update = {
            button.text =
                listOf(title, value()).filter { it.isNotEmpty() }.joinToString("\n")
        }
        refreshers.add(update)
        update()
        rows.addView(button, LinearLayout.LayoutParams(-1, -2))
    }

    private fun persist(edit: () -> Unit) {
        val before = config.encode()
        try {
            edit()
            GameSettingsStore.save(id, config)
        } catch (e: Exception) {
            config = GameGraphicsConfig(before)
            Toast.makeText(
                this,
                getString(R.string.game_settings_error, e.message),
                Toast.LENGTH_LONG
            ).show()
        }
        refreshers.forEach { it() }
    }

    private fun select(
        title: String,
        section: String,
        key: String,
        names: List<String>,
        values: List<String>
    ) {
        val options = listOf(getString(R.string.game_setting_use_global)) + names
        val selected = config.getOverride(section, key)?.let { values.indexOf(it) + 1 } ?: 0
        MaterialAlertDialogBuilder(this).setTitle(title)
            .setSingleChoiceItems(options.toTypedArray(), selected) { dialog, which ->
                persist { config.set(section, key, if (which == 0) null else values[which - 1]) }
                dialog.dismiss()
            }.setNegativeButton(android.R.string.cancel, null).show()
    }

    private fun choice(setting: IntSetting, title: Int, namesId: Int, valuesId: Int) {
        val names = resources.getStringArray(namesId).toList()
        val values = resources.getIntArray(valuesId).map { it.toString() }
        row(getString(title), {
            val value = effective(setting, setting.defaultValue.toString())
            summary(setting.section, setting.key, names.getOrNull(values.indexOf(value)) ?: value)
        }) { select(getString(title), setting.section, setting.key, names, values) }
    }

    private fun toggle(setting: BooleanSetting, title: Int) {
        val names =
            listOf(getString(R.string.game_setting_on), getString(R.string.game_setting_off))
        val values = listOf("true", "false")
        row(getString(title), {
            val value = effective(setting, setting.defaultValue.toString())
            summary(
                setting.section,
                setting.key,
                names[
                    if (value == "true" ||
                        value == "1"
                    ) {
                        0
                    } else {
                        1
                    }
                ]
            )
        }) { select(getString(title), setting.section, setting.key, names, values) }
    }

    private fun number(setting: AbstractSetting, title: Int, min: Double, max: Double) {
        val integral = setting is IntSetting || setting == FloatSetting.SECOND_SCREEN_OPACITY
        val fallback = when (setting) {
            is IntSetting -> setting.defaultValue.toString()
            is FloatSetting -> setting.defaultValue.toString()
            else -> error("Unsupported numeric setting")
        }
        row(getString(title), {
            summary(setting.section!!, setting.key!!, effective(setting, fallback))
        }) {
            val layout = TextInputLayout(this).apply { hint = "$min … $max" }
            val input = TextInputEditText(layout.context).apply {
                inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_SIGNED or
                    if (!integral) InputType.TYPE_NUMBER_FLAG_DECIMAL else 0
                setText(effective(setting, fallback))
            }
            layout.addView(input)
            val dialog = MaterialAlertDialogBuilder(this).setTitle(title).setView(layout)
                .setNegativeButton(android.R.string.cancel, null)
                .setNeutralButton(R.string.game_setting_use_global) { _, _ ->
                    persist { config.set(setting.section!!, setting.key!!, null) }
                }.setPositiveButton(android.R.string.ok, null).create()
            dialog.setOnShowListener {
                dialog.getButton(
                    android.content.DialogInterface.BUTTON_POSITIVE
                ).setOnClickListener {
                    val value = input.text.toString().toDoubleOrNull()
                    if (value == null || !value.isFinite() || value !in min..max ||
                        (integral && value != value.toInt().toDouble())
                    ) {
                        layout.error =
                            getString(
                                R.string.game_setting_number_error,
                                min.toString(),
                                max.toString()
                            )
                    } else {
                        val encoded = if (integral) {
                            value.toInt().toString()
                        } else {
                            value.toFloat().toString()
                        }
                        persist {
                            config.set(
                                setting.section!!,
                                setting.key!!,
                                encoded
                            )
                        }
                        dialog.dismiss()
                    }
                }
            }
            dialog.show()
        }
    }

    private fun driverRow() {
        row(getString(R.string.game_gpu_driver), {
            val selected = config.getOverride("GpuDriver", "selection")
            when (selected) {
                null -> getString(
                    R.string.game_setting_inherited,
                    GpuDriverHelper.customDriverData.name ?: getString(R.string.system_gpu_driver)
                )

                "system" -> getString(
                    R.string.game_setting_override,
                    getString(R.string.system_gpu_driver)
                )

                else -> getString(
                    R.string.game_setting_override,
                    drivers.firstOrNull { it.first.toString() == selected }?.second?.name
                        ?: getString(R.string.game_driver_missing)
                )
            }
        }) {
            if (!GpuDriverHelper.supportsCustomDriverLoading()) {
                Toast.makeText(this, R.string.game_driver_unsupported, Toast.LENGTH_LONG).show()
            } else {
                select(
                    getString(R.string.game_gpu_driver),
                    "GpuDriver",
                    "selection",
                    listOf(getString(R.string.system_gpu_driver)) +
                        drivers.drop(1).map { it.second.name!! },
                    listOf("system") + drivers.drop(1).map { it.first.toString() }
                )
            }
        }
    }

    companion object {
        fun launch(context: Context, game: Game) {
            context.startActivity(
                Intent(context, GameGraphicsActivity::class.java)
                    .putExtra(
                        "game_settings_id",
                        GameSettingsStore.id(game)
                    ).putExtra("game_title", game.title)
            )
        }
    }
}
