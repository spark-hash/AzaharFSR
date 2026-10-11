// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

import android.os.Build
import android.view.Display
import android.view.Surface
import org.citra.citra_emu.NativeLibrary

/** Refresh rates belong to the individual displays, including Thor's second panel. */
object LsfgDisplay {
    private val previousHints = floatArrayOf(-1f, -1f)
    private val previousSurfaces = arrayOfNulls<Surface>(2)

    fun update(surface: Surface, display: Display?, secondary: Boolean) {
        if (display == null || !surface.isValid) return
        NativeLibrary.setLsfgDisplayRefreshRate(display.refreshRate.toDouble(), secondary)
        val mode = NativeLibrary.getActiveLsfgMode()
        val target = if (mode == 2) {
            120f
        } else if (mode == 1) {
            60f
        } else {
            0f
        }
        val index = if (secondary) 1 else 0
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
            (previousHints[index] != target || previousSurfaces[index] !== surface)
        ) {
            runCatching { surface.setFrameRate(target, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT) }
                .onSuccess {
                    previousHints[index] = target
                    previousSurfaces[index] = surface
                }
        }
    }
}
