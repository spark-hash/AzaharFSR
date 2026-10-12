// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PresentationRatesTest {
    private fun stats(real: Double, generated: Double, source: Double) =
        doubleArrayOf(real, generated, 0.0, 3.0, 2.0, source)

    @Test fun interpolationDoesNotIncreaseSourceRate() {
        val rates = PresentationRates.measure(stats(30.0, 30.0, 30.0), stats(0.0, 0.0, 0.0), 1.0)!!
        assertEquals(30.0, rates.source, 0.0)
        assertEquals(60.0, rates.output, 0.0)
    }

    @Test fun repeatedStaticFramesOnlyIncreaseOutputRate() {
        val rates = PresentationRates.measure(stats(60.0, 0.0, 0.0), stats(0.0, 0.0, 0.0), 1.0)!!
        assertEquals(0.0, rates.source, 0.0)
        assertEquals(60.0, rates.output, 0.0)
    }

    @Test fun nativeFramesUseElapsedWallTime() {
        val rates = PresentationRates.measure(stats(120.0, 0.0, 120.0), stats(0.0, 0.0, 0.0), 2.0)!!
        assertEquals(60.0, rates.source, 0.0)
        assertEquals(60.0, rates.output, 0.0)
    }

    @Test fun counterResetDoesNotProduceNegativeRates() {
        val rates = PresentationRates.measure(stats(0.0, 0.0, 0.0), stats(90.0, 90.0, 90.0), 1.0)!!
        assertEquals(0.0, rates.source, 0.0)
        assertEquals(0.0, rates.output, 0.0)
    }

    @Test fun unavailableCountersAreNotInvented() {
        assertNull(PresentationRates.measure(stats(0.0, 0.0, -1.0), stats(0.0, 0.0, 0.0), 1.0))
        assertNull(PresentationRates.measure(stats(0.0, 0.0, 0.0), stats(0.0, 0.0, 0.0), 0.0))
    }
}
