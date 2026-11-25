// app/presets.hpp: presets and Copy / Paste Settings
#pragma once

#include <algorithm> // std::min
#include <cstddef>   // size_t
#include <string>    // std::string
#include <utility>   // std::move

#include "app/constants.hpp"     // kPresetRows
#include "app/editor_state.hpp"  // inline EditorState state, setStatus
#include "app/saved_edits.hpp"   // pictureLabel
#include "app/ui_state.hpp"      // uiState, PresetEntry
#include "io/file_utils.hpp"     // baseName
#include "io/preset.hpp"         // Preset, kPresetNameMax, presetDir, listPresetFiles,
                                 // loadPresetFile, savePresetFile, deletePresetFile,
                                 // sanitizePresetName

// ---------------- Presets ----------------
// A preset is a saved look (sliders, curves, Color Mixer, Color Grading; see
// io/preset.hpp). They live as small files in presetDir(); the list
// (uiState.presets.list) is a cache of what is there, re-read when the pane is opened and after every save
// or delete (never per frame).
inline void refreshPresets(const std::string &select = std::string()) {
  uiState.presets.list.clear();
  uiState.presets.sel = -1;
  const std::string dir = presetDir();
  for (const std::string &fn : listPresetFiles(dir)) {
    if (uiState.presets.list.size() >= kPresetRows)
      break;
    Preset p;
    std::string err;
    if (!loadPresetFile(dir + "/" + fn, &p, &err))
      continue; // a damaged file is skipped, not fatal
    uiState.presets.list.push_back({p.name, dir + "/" + fn, std::move(p)});
    if (!select.empty() && uiState.presets.list.back().name == select)
      uiState.presets.sel = static_cast<int>(uiState.presets.list.size()) - 1;
  }
}

// Replaces the look with preset i: one undo step, "Preset: <name>" in History.
// Crop, rotation and masks are not touched.
inline void applyPresetEntry(size_t i) {
  if (state.dragging || !state.original || i >= uiState.presets.list.size())
    return;
  uiState.presets.sel = static_cast<int>(i);
  const PresetEntry &e = uiState.presets.list[i];
  if (!state.adjust.applyPreset(e.preset)) {
    setStatus("Already looks like \"" + e.name + "\"", false);
    return;
  }
  uiState.dropCurveSelection(); // the preset replaced the curves
  state.markAdjustDirty(); // re-render through the normal (drag-safe) path
  state.markDirty();
  setStatus("Applied preset: " + e.name, false);
}

// Saves the current look under the name typed in the field.
inline void saveCurrentPreset() {
  if (!state.original)
    return;
  const std::string name = sanitizePresetName(
      uiState.names.preset ? uiState.names.preset->text : std::string());
  if (name.empty()) {
    setStatus("Type a name for the preset first", true);
    return;
  }
  std::string err;
  if (!savePresetFile(presetDir(), state.adjust.currentPreset(name), &err)) {
    setStatus("Couldn't save preset: " + err, true);
    return;
  }
  refreshPresets(name);
  if (uiState.names.preset) {
    uiState.names.preset->text.clear();
    uiState.names.preset->cursor = 0;
    uiState.names.preset->scrollOffset = 0.0f;
    uiState.names.preset->dirty = true;
  }
  setStatus("Saved preset: " + name, false);
}

inline void deleteSelectedPreset() {
  if (uiState.presets.sel < 0 || static_cast<size_t>(uiState.presets.sel) >= uiState.presets.list.size())
    return;
  const PresetEntry e = uiState.presets.list[static_cast<size_t>(uiState.presets.sel)];
  if (!deletePresetFile(e.path)) {
    setStatus("Couldn't delete " + baseName(e.path), true);
    return;
  }
  refreshPresets();
  setStatus("Deleted preset: " + e.name, false);
}

// Keeps the name field to printable ASCII (the TextInput is ASCII-only) and
// the preset name cap. Trimming happens on save, so typing spaces between
// words still works.
inline void filterPresetNameField(const std::string &typed) {
  std::string s;
  for (char c : typed)
    if (c >= 0x20 && c <= 0x7E && s.size() < kPresetNameMax)
      s.push_back(c);
  if (uiState.names.preset && uiState.names.preset->text != s) {
    uiState.names.preset->text = s;
    uiState.names.preset->cursor = std::min(uiState.names.preset->cursor, s.size());
    uiState.names.preset->dirty = true;
  }
}

// ---------------- Copy / Paste Settings ----------------
// Same payload as a preset (the look only), kept in memory instead of a file.

inline void copySettings() {
  if (!state.original)
    return;
  uiState.clipboard.settings = state.adjust.currentPreset(std::string());
  uiState.clipboard.settingsFrom = pictureLabel(state.imagePath);
  setStatus("Copied settings from " + uiState.clipboard.settingsFrom, false);
}

inline void pasteSettings() {
  if (state.dragging || !state.original)
    return;
  if (!uiState.clipboard.settings) {
    setStatus("Copy settings from a photo first", true);
    return;
  }
  Preset p = *uiState.clipboard.settings;
  p.name = "Pasted"; // History shows "Preset: Pasted"
  if (!state.adjust.applyPreset(p)) {
    setStatus("Already has these settings", false);
    return;
  }
  uiState.dropCurveSelection();
  state.markAdjustDirty();
  state.markDirty();
  setStatus("Pasted settings from " + uiState.clipboard.settingsFrom, false);
}

