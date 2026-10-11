// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.cheats.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class FpsCheatParserTest {
    @Test fun keepsMultilineAuthorAndSpeedWarning() {
        val result = FpsCheatParser.parse(
            "[60FPS]\n201EC238 00000000\n{Game runs faster.\nCodes by Example}\n"
        )
        assertEquals(1, result.size)
        assertEquals("Game runs faster.\nCodes by Example", result[0].notes)
        assertTrue(FpsCheatParser.parse("[60FPS]\n201EC238 00000000\n{unfinished").isEmpty())
    }

    @Test fun matchesExactRegionalTitleId() {
        assertEquals("0004000000086300", FpsCheatParser.titleIdHex(0x0004000000086300))
        assertTrue(
            FpsCheatParser.matchesTitle(
                "Cheats/Game (USA)/0004000000086300.txt",
                0x0004000000086300
            )
        )
        assertFalse(
            FpsCheatParser.matchesTitle(
                "Cheats/Game (EUR)/0004000000086400.txt",
                0x0004000000086300
            )
        )
        assertFalse(
            FpsCheatParser.matchesTitle("Cheats/0004000000086300.txt.bak", 0x0004000000086300)
        )
    }

    @Test fun extractsSixtyFpsAndKeepsVersionAndAuthor() {
        val result = FpsCheatParser.parse(
            """
            [30FPS (Hold R for 60FPS)]
            00100000 00000002
            [++FPS++]
            [++v1.2++]
            [60FPS]
            00100000 00000001
            {Codes by Example}
            [30FPS]
            00100000 00000002
            [--]
            [++v1.0++]
            [60++FPS]
            00100004 00000001
            """.trimIndent()
        )
        assertEquals(2, result.size)
        assertEquals("FPS / v1.2 / 60FPS", result[0].name)
        assertEquals("Codes by Example", result[0].notes)
        assertEquals("FPS / v1.0 / 60++FPS", result[1].name)
    }

    @Test fun handlesBomCrLfCommentsAndDuplicates() {
        val result = FpsCheatParser.parse(
            "\uFEFF[60 FPS]\r\n00abcdef   00000001 // explanation\r\n*author\r\n[60FPS duplicate]\r\n00ABCDEF 00000001\r\n"
        )
        assertEquals(1, result.size)
        assertEquals("00ABCDEF 00000001", result[0].code)
        assertEquals("author", result[0].notes)
    }

    @Test fun rejectsEntireUnsupportedOrMalformedSection() {
        for (bad in listOf(
            "not a code",
            "F0000000 00000001",
            "DE000000 00000000",
            "D2000000 00000001"
        )) {
            assertTrue(FpsCheatParser.parse("[60FPS]\n00100000 00000001\n$bad").isEmpty())
        }
        assertTrue(FpsCheatParser.parse("[60FPS]\n").isEmpty())
    }

    @Test fun validatesEmbeddedPatchLengthWithoutTreatingDataAsOpcodes() {
        assertTrue(FpsCheatParser.parse("[60FPS]\nE0100000 00000010\nFFFFFFFF FFFFFFFF").isEmpty())
        val result = FpsCheatParser.parse(
            "[60FPS]\nE0100000 00000008\nFFFFFFFF DEADBEEF\nD2000000 00000000"
        )
        assertEquals(1, result.size)
    }

    @Test fun retainsHoldButtonSixtyFpsVariant() {
        val result = FpsCheatParser.parse(
            "[60FPS v1.0 (Hold R for 30FPS)]\n00100000 00000001\nDD000000 00000100\n00100000 00000002\nD0000000 00000000"
        )
        assertEquals(1, result.size)
        assertTrue(result[0].code.contains("DD000000 00000100"))
    }
}
