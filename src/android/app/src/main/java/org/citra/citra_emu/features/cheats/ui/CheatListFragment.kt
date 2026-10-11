// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

package org.citra.citra_emu.features.cheats.ui

import android.annotation.SuppressLint
import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.divider.MaterialDividerItemDecoration
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.citra.citra_emu.R
import org.citra.citra_emu.databinding.FragmentCheatListBinding
import org.citra.citra_emu.features.cheats.model.Cheat
import org.citra.citra_emu.features.cheats.model.CheatsViewModel
import org.citra.citra_emu.features.cheats.model.FpsCheatDownload
import org.citra.citra_emu.features.cheats.model.FpsCheatParser
import org.citra.citra_emu.features.cheats.model.FpsCheatRepository
import org.citra.citra_emu.ui.main.MainActivity

class CheatListFragment : Fragment() {
    private var _binding: FragmentCheatListBinding? = null
    private val binding get() = _binding!!

    private val cheatsViewModel: CheatsViewModel by activityViewModels()
    private var fpsDialog: Dialog? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCheatListBinding.inflate(layoutInflater)
        return binding.root
    }

    // This is using the correct scope, lint is just acting up
    @SuppressLint("UnsafeRepeatOnLifecycleDetector")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.cheatList.adapter = CheatsAdapter(requireActivity(), cheatsViewModel)
        binding.cheatList.layoutManager = LinearLayoutManager(requireContext())
        binding.cheatList.addItemDecoration(
            MaterialDividerItemDecoration(
                requireContext(),
                MaterialDividerItemDecoration.VERTICAL
            )
        )

        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.cheatAddedEvent.collect { position: Int? ->
                        position?.let {
                            binding.cheatList.apply {
                                post { (adapter as CheatsAdapter).notifyItemInserted(it) }
                            }
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.cheatChangedEvent.collect { position: Int? ->
                        position?.let {
                            binding.cheatList.apply {
                                post { (adapter as CheatsAdapter).notifyItemChanged(it) }
                            }
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.cheatDeletedEvent.collect { position: Int? ->
                        position?.let {
                            binding.cheatList.apply {
                                post { (adapter as CheatsAdapter).notifyItemRemoved(it) }
                            }
                        }
                    }
                }
            }
        }

        binding.fab.setOnClickListener {
            cheatsViewModel.startAddingCheat()
            cheatsViewModel.openDetailsView()
        }

        binding.toolbarCheatList.setNavigationOnClickListener {
            if (requireActivity() is MainActivity) {
                view.findNavController().popBackStack()
            } else {
                requireActivity().finish()
            }
        }

        binding.toolbarCheatList.inflateMenu(R.menu.menu_fps_cheats)
        binding.toolbarCheatList.setOnMenuItemClickListener {
            if (it.itemId == R.id.menu_download_fps_cheats) {
                downloadFpsCheats()
                true
            } else {
                false
            }
        }

        setInsets()
    }

    override fun onDestroyView() {
        fpsDialog?.dismiss()
        fpsDialog = null
        super.onDestroyView()
        _binding = null
    }

    private fun downloadFpsCheats() {
        if (cheatsViewModel.isEditing.value) {
            fpsDialog = MaterialAlertDialogBuilder(requireContext())
                .setMessage(R.string.fps_cheats_finish_edit)
                .setPositiveButton(android.R.string.ok, null).show()
            return
        }
        val titleId = cheatsViewModel.currentTitleId
        val item = binding.toolbarCheatList.menu.findItem(R.id.menu_download_fps_cheats)
        if (!item.isEnabled) return
        item.isEnabled = false
        item.setTitle(R.string.fps_cheats_downloading)
        viewLifecycleOwner.lifecycleScope.launch {
            try {
                val candidates = withContext(Dispatchers.IO) {
                    FpsCheatRepository.download(titleId)
                }
                    .filter { Cheat.isValidGatewayCode(it.code) == 0 }
                if (cheatsViewModel.currentTitleId != titleId) return@launch
                if (candidates.isEmpty()) {
                    fpsDialog = MaterialAlertDialogBuilder(requireContext())
                        .setMessage(
                            getString(
                                R.string.fps_cheats_not_found,
                                FpsCheatParser.titleIdHex(titleId)
                            )
                        )
                        .setPositiveButton(android.R.string.ok, null).show()
                } else {
                    fpsDialog = MaterialAlertDialogBuilder(requireContext())
                        .setTitle(
                            getString(R.string.fps_cheats_found, FpsCheatParser.titleIdHex(titleId))
                        )
                        .setItems(candidates.map { it.name }.toTypedArray()) { _, position ->
                            previewFpsCheat(titleId, candidates[position])
                        }
                        .setNegativeButton(android.R.string.cancel, null).show()
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (_: Exception) {
                fpsDialog = MaterialAlertDialogBuilder(requireContext())
                    .setMessage(R.string.fps_cheats_download_failed)
                    .setPositiveButton(android.R.string.ok, null).show()
            } finally {
                item.isEnabled = true
                item.setTitle(R.string.fps_cheats_download)
            }
        }
    }

    private fun previewFpsCheat(titleId: Long, cheat: FpsCheatDownload) {
        fpsDialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(cheat.name)
            .setMessage(getString(R.string.fps_cheats_preview, cheat.notes, cheat.code))
            .setPositiveButton(R.string.fps_cheats_import_enable) { _, _ ->
                importFpsCheat(titleId, cheat, true)
            }
            .setNeutralButton(R.string.fps_cheats_import_only) { _, _ ->
                importFpsCheat(titleId, cheat, false)
            }
            .setNegativeButton(android.R.string.cancel, null).show()
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun importFpsCheat(titleId: Long, cheat: FpsCheatDownload, enable: Boolean) {
        if (cheatsViewModel.currentTitleId != titleId || cheatsViewModel.isEditing.value) return
        cheatsViewModel.importFpsCheat(titleId, cheat, enable)
        binding.cheatList.adapter?.notifyDataSetChanged()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val mlpAppBar = binding.toolbarCheatList.layoutParams as MarginLayoutParams
            mlpAppBar.leftMargin = leftInsets
            mlpAppBar.rightMargin = rightInsets
            binding.toolbarCheatList.layoutParams = mlpAppBar

            binding.cheatList.updatePadding(
                left = leftInsets,
                right = rightInsets,
                bottom = barInsets.bottom +
                    resources.getDimensionPixelSize(R.dimen.spacing_fab_list)
            )

            val mlpFab = binding.fab.layoutParams as MarginLayoutParams
            val fabPadding = resources.getDimensionPixelSize(R.dimen.spacing_large)
            mlpFab.leftMargin = leftInsets + fabPadding
            mlpFab.bottomMargin = barInsets.bottom + fabPadding
            mlpFab.rightMargin = rightInsets + fabPadding
            binding.fab.layoutParams = mlpFab
            windowInsets
        }
    }
}
