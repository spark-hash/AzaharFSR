// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

import android.os.Build
import android.view.Display
import android.view.Surface
import android.view.Window
import org.citra.citra_emu.NativeLibrary

/** Refresh rates belong to the individual displays, including Thor's second panel. */
object LsfgDisplay {
    private val previousHints = floatArrayOf(-1f, -1f)
    private val previousSurfaces = arrayOfNulls<Surface>(2)

    fun update(surface: Surface, display: Display?, secondary: Boolean, window: Window?) {
        if (display == null || !surface.isValid) return
        NativeLibrary.setLsfgDisplayRefreshRate(display.refreshRate.toDouble(), secondary)
        val mode = NativeLibrary.getActiveLsfgMode(secondary)
        val target = if (mode != 0) {
            60f
        } else {
            0f
        }
        val index = if (secondary) 1 else 0
        val currentMode = display.mode
        val preferredMode = display.supportedModes
            .filter {
                it.physicalWidth == currentMode.physicalWidth &&
                    it.physicalHeight == currentMode.physicalHeight &&
                    it.refreshRate <= target + 0.5f
            }
            .maxByOrNull { it.refreshRate }
        window?.let {
            val attributes = it.attributes
            val modeId = if (target > 0f) preferredMode?.modeId ?: 0 else 0
            val rate = if (target > 0f) preferredMode?.refreshRate ?: target else 0f
            if (attributes.preferredDisplayModeId != modeId ||
                attributes.preferredRefreshRate != rate
            ) {
                attributes.preferredDisplayModeId = modeId
                attributes.preferredRefreshRate = rate
                it.attributes = attributes
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            (previousHints[index] != target || previousSurfaces[index] !== surface)
        ) {
            runCatching {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    surface.setFrameRate(
                        target,
                        Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                        Surface.CHANGE_FRAME_RATE_ALWAYS
                    )
                } else {
                    surface.setFrameRate(target, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT)
                }
            }
                .onSuccess {
                    previousHints[index] = target
                    previousSurfaces[index] = surface
                }
        }
    }
}
