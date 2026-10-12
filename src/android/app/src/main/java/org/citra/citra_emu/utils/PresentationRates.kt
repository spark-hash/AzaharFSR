// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

/** GSP submissions and window presentations are separate measurements, including repeated images. */
object PresentationRates {
    data class Sample(val source: Double, val output: Double)

    fun measure(stats: DoubleArray, last: DoubleArray, seconds: Double): Sample? {
        if (stats.size != 6 || last.size != 6 || !seconds.isFinite() || seconds <= 0) return null
        if (listOf(0, 1, 5).any { !stats[it].isFinite() || !last[it].isFinite() }) return null
        if (stats[5] < 0 || last[5] < 0) return null
        val source = (stats[5] - last[5]).coerceAtLeast(0.0) / seconds
        val output = (stats[0] - last[0] + stats[1] - last[1]).coerceAtLeast(0.0) / seconds
        return Sample(source, output)
    }
}
