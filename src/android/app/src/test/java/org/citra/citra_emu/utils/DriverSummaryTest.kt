// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.utils

import org.junit.Assert.assertEquals
import org.junit.Test

class DriverSummaryTest {
    @Test fun runningTurnipIsRecognizedWithoutArchiveMetadata() {
        assertEquals("turnip", DriverSummary.family("turnip Mesa driver", "25.1.0"))
        assertEquals("25.1.0", DriverSummary.version("Vulkan", "25.1.0"))
    }

    @Test fun qualcommGlVersionUsesVendorVersionInsteadOfApiVersion() {
        assertEquals("qualcomm", DriverSummary.family("Qualcomm", "OpenGL ES 3.2 V@615.0"))
        assertEquals("615.0", DriverSummary.version("OpenGL", "OpenGL ES 3.2 V@615.0 (build)"))
    }

    @Test fun mesaGlVersionIsExtracted() {
        assertEquals(
            "24.3.0-devel",
            DriverSummary.version("OpenGL", "OpenGL ES 3.2 Mesa 24.3.0-devel")
        )
    }

    @Test fun unknownGlDriverDoesNotClaimApiVersionAsDriverVersion() {
        assertEquals("ARM", DriverSummary.family("ARM", "OpenGL ES 3.2"))
        assertEquals("", DriverSummary.version("OpenGL", "OpenGL ES 3.2"))
    }

    @Test fun vulkanVersionAndUnknownNamesAreBounded() {
        assertEquals("512.744.0", DriverSummary.version("Vulkan", "512.744.0"))
        assertEquals(16, DriverSummary.family("abcdefghijklmnopqrst", "").length)
        assertEquals("", DriverSummary.family("", ""))
    }
}
