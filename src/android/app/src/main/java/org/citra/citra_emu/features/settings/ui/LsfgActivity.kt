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
import org.citra.citra_emu.utils.LsfgHelper
import org.citra.citra_emu.utils.ThemeUtil

class LsfgActivity : AppCompatActivity() {
    private lateinit var status: TextView
    private lateinit var importButton: MaterialButton
    private val pickDll = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            lifecycleScope.launch {
                importButton.isEnabled = false
                status.setText(R.string.lsfg_importing)
                try {
                    withContext(Dispatchers.IO) { LsfgHelper.importDll(uri) }
                    refresh()
                } catch (cancelled: CancellationException) {
                    throw cancelled
                } catch (error: Exception) {
                    refresh()
                    MaterialAlertDialogBuilder(this@LsfgActivity)
                        .setMessage(getString(R.string.lsfg_import_failed, error.message))
                        .setPositiveButton(android.R.string.ok, null).show()
                } finally {
                    importButton.isEnabled = true
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
                setTitle(R.string.lsfg_assets)
                setNavigationIcon(R.drawable.ic_back)
                setNavigationOnClickListener { finish() }
            }
        )
        val rows = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val padding = (16 * resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
        }
        rows.addView(TextView(this).apply { setText(R.string.lsfg_import_description) })
        status = TextView(this)
        rows.addView(status)
        importButton = MaterialButton(this).apply {
            setText(R.string.lsfg_import)
            setOnClickListener { pickDll.launch(arrayOf("*/*")) }
        }
        rows.addView(importButton)
        root.addView(ScrollView(this).apply { addView(rows) }, LinearLayout.LayoutParams(-1, 0, 1f))
        setContentView(root)
        refresh()
    }

    private fun refresh() {
        status.setText(if (LsfgHelper.isReady()) R.string.lsfg_ready else R.string.lsfg_missing)
    }
}
