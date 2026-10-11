// SPDX-License-Identifier: GPL-2.0-or-later
package org.citra.citra_emu.features.settings.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GameGraphicsConfigTest {
    @Test fun overridesDoNotMutateGlobalOrAnotherGame() {
        val global = GameGraphicsConfig("[Renderer]\nresolution_factor=2\nfsr_enabled=false\n")
        val before = global.encode()
        val gameA = GameGraphicsConfig()
        val gameB = GameGraphicsConfig()
        gameA.set("Renderer", "resolution_factor", "4")
        assertEquals("4", gameA.effective("Renderer", "resolution_factor", global, "1"))
        assertEquals("2", gameB.effective("Renderer", "resolution_factor", global, "1"))
        assertEquals(before, global.encode())
        assertNull(gameA.getOverride("Renderer", "fsr_enabled"))
    }

    @Test fun inheritedValuesFollowFutureGlobalChanges() {
        val global = GameGraphicsConfig()
        val game = GameGraphicsConfig()
        assertEquals("1", game.effective("Renderer", "resolution_factor", global, "1"))
        global.set("Renderer", "resolution_factor", "3")
        assertEquals("3", game.effective("Renderer", "resolution_factor", global, "1"))
        game.set("Renderer", "resolution_factor", "5")
        global.set("Renderer", "resolution_factor", "2")
        assertEquals("5", game.effective("Renderer", "resolution_factor", global, "1"))
        game.set("Renderer", "resolution_factor", null)
        assertEquals("2", game.effective("Renderer", "resolution_factor", global, "1"))
    }

    @Test fun restartPreservesExplicitFalseAndDriverSelection() {
        val game = GameGraphicsConfig()
        game.set("Renderer", "fsr_enabled", "false")
        game.set("GpuDriver", "selection", "content://provider/tree/storage/document/driver.zip")
        val restored = GameGraphicsConfig(game.encode())
        assertEquals("false", restored.getOverride("Renderer", "fsr_enabled"))
        assertEquals(
            game.getOverride("GpuDriver", "selection"),
            restored.getOverride("GpuDriver", "selection")
        )
        assertNull(restored.getOverride("Renderer", "anti_aliasing"))
    }

    @Test fun systemDriverIsDistinctFromInheritanceAndResetClearsBoth() {
        val game = GameGraphicsConfig()
        assertNull(game.getOverride("GpuDriver", "selection"))
        game.set("GpuDriver", "selection", "system")
        game.set("Utility", "custom_textures", "true")
        assertEquals("system", game.getOverride("GpuDriver", "selection"))
        game.clear()
        val restored = GameGraphicsConfig(game.encode())
        assertNull(restored.getOverride("GpuDriver", "selection"))
        assertNull(restored.getOverride("Utility", "custom_textures"))
    }

    @Test fun uneditedValuesSurviveSelectiveUpdates() {
        val game =
            GameGraphicsConfig(
                "[Renderer]\nresolution_factor=3\nanti_aliasing=2\n[GpuDriver]\nselection=system\n"
            )
        game.set("Renderer", "resolution_factor", null)
        val restored = GameGraphicsConfig(game.encode())
        assertEquals("2", restored.getOverride("Renderer", "anti_aliasing"))
        assertEquals("system", restored.getOverride("GpuDriver", "selection"))
        assertNull(restored.getOverride("Renderer", "resolution_factor"))
    }

    @Test fun legacyFsrMigratesAndCanReturnToInheritance() {
        val global = GameGraphicsConfig("[Renderer]\nupscaling_mode=2\n")
        val game = GameGraphicsConfig("[Renderer]\nfsr_enabled=true\n")
        assertEquals("1", game.effective("Renderer", "upscaling_mode", global, "0"))
        val restarted = GameGraphicsConfig(game.encode())
        assertEquals("1", restarted.getOverride("Renderer", "upscaling_mode"))
        restarted.set("Renderer", "upscaling_mode", null)
        assertEquals("2", restarted.effective("Renderer", "upscaling_mode", global, "0"))
        assertNull(restarted.getOverride("Renderer", "fsr_enabled"))
    }

    @Test fun explicitUpscalerWinsOverLegacyFsrAndPreservesStrength() {
        val game =
            GameGraphicsConfig(
                "[Renderer]\nfsr_enabled=true\nupscaling_mode=0\nanime4k_strength=75\n"
            )
        assertEquals("0", game.getOverride("Renderer", "upscaling_mode"))
        game.set("Renderer", "upscaling_mode", "2")
        val restarted = GameGraphicsConfig(game.encode())
        assertEquals("2", restarted.getOverride("Renderer", "upscaling_mode"))
        assertEquals("75", restarted.getOverride("Renderer", "anime4k_strength"))
        assertNull(restarted.getOverride("Renderer", "fsr_enabled"))
        val off = GameGraphicsConfig("[Renderer]\nfsr_enabled=false\n")
        assertEquals("0", off.getOverride("Renderer", "upscaling_mode"))
    }

    @Test fun reloadingCurrentGameKeepsOverridesAfterGlobalEdits() {
        val global = GameGraphicsConfig("[Renderer]\nupscaling_mode=1\nfsr_sharpness=50\n")
        val game = GameGraphicsConfig("[Renderer]\nupscaling_mode=2\nfsr_sharpness=70\n")
        game.set("Renderer", "fsr_sharpness", "30")
        global.set("Renderer", "upscaling_mode", "0")
        global.set("Renderer", "fsr_sharpness", "100")
        val reloaded = GameGraphicsConfig(game.encode())
        assertEquals("2", reloaded.effective("Renderer", "upscaling_mode", global, "0"))
        assertEquals("30", reloaded.effective("Renderer", "fsr_sharpness", global, "50"))
        reloaded.set("Renderer", "fsr_sharpness", null)
        assertEquals("100", reloaded.effective("Renderer", "fsr_sharpness", global, "50"))
    }

    @Test fun lsfgOverrideSurvivesReloadAndResetRestoresInheritance() {
        val global = GameGraphicsConfig("[Renderer]\nlsfg_mode=1\n")
        val game = GameGraphicsConfig()
        assertEquals("1", game.effective("Renderer", "lsfg_mode", global, "0"))
        game.set("Renderer", "lsfg_mode", "0")
        val restored = GameGraphicsConfig(game.encode())
        assertEquals("0", restored.effective("Renderer", "lsfg_mode", global, "0"))
        restored.set("Renderer", "lsfg_mode", null)
        assertEquals("1", restored.effective("Renderer", "lsfg_mode", global, "0"))
    }
}
