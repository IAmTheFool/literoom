// app/snapshots.hpp: named snapshots
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Snapshots ----------------
// A snapshot is a named, complete copy of the picture's edits (sliders, curves,
// Color Mix, Color Grading, crop, rotation, straighten AND masks; see
// NamedSnapshot in io/sidecar.hpp). They live in state.snapshots, are saved in
// the picture's sidecar with everything else (snapshotsDirty makes autosave
// write when only the list changed), and are not edits themselves: adding or
// deleting one is not an undo step. Restoring one IS: one step, "Snapshot:
// <n>".

// "Snapshot 1", "Snapshot 2", ...: the first one no snapshot already uses.
inline std::string nextSnapshotName() {
  for (size_t n = 1; n <= kMaxSnapshots + 1; ++n) {
    const std::string cand = "Snapshot " + std::to_string(n);
    bool used = false;
    for (const NamedSnapshot &s : state.snapshots)
      used = used || s.name == cand;
    if (!used)
      return cand;
  }
  return "Snapshot";
}

// Keeps the name field to printable ASCII and the name cap (trimming happens
// on save, so typing spaces between words still works).
inline void filterSnapshotNameField(const std::string &typed) {
  std::string s;
  for (char c : typed)
    if (c >= 0x20 && c <= 0x7E && s.size() < kSnapshotNameMax)
      s.push_back(c);
  if (uiState.names.snapshot && uiState.names.snapshot->text != s) {
    uiState.names.snapshot->text = s;
    uiState.names.snapshot->cursor = std::min(uiState.names.snapshot->cursor, s.size());
    uiState.names.snapshot->dirty = true;
  }
}

// Saves the picture's current edits as a snapshot named like the field (or
// "Snapshot <n>" when it is blank).
inline void addSnapshot() {
  if (!state.original)
    return;
  if (state.snapshots.size() >= kMaxSnapshots) {
    setStatus("A picture holds at most " + std::to_string(kMaxSnapshots) +
                  " snapshots: delete one first",
              true);
    return;
  }
  std::string name = sanitizeSnapshotName(uiState.names.snapshot ? uiState.names.snapshot->text
                                                         : std::string());
  if (name.empty())
    name = nextSnapshotName();
  NamedSnapshot s;
  s.name = name;
  s.edits = state.adjust.currentEdits();
  // Every snapshot repeats every mask (object bitmaps included) in the
  // sidecar, so the list is bounded in bytes as well as in count.
  SnapshotList trial = state.snapshots;
  trial.push_back(s);
  if (sidecarSnapshotsToJson(trial).dump().size() > kSnapshotsMaxBytes) {
    setStatus("Too many masks to keep another snapshot: delete one first",
              true);
    return;
  }
  state.snapshots = std::move(trial);
  state.snapshotsDirty = true; // autosave writes it
  uiState.snapshots.sel = static_cast<int>(state.snapshots.size()) - 1;
  if (uiState.names.snapshot) {
    uiState.names.snapshot->text.clear();
    uiState.names.snapshot->cursor = 0;
    uiState.names.snapshot->scrollOffset = 0.0f;
    uiState.names.snapshot->dirty = true;
  }
  state.markDirty();
  setStatus("Saved snapshot: " + name, false);
}

// Puts the picture back as snapshot i describes it: one undo step. Geometry and
// masks come back too, so the picture is refreshed exactly as after an undo.
inline void applySnapshotEntry(size_t i) {
  if (state.dragging || !state.original || i >= state.snapshots.size())
    return;
  uiState.snapshots.sel = static_cast<int>(i);
  const NamedSnapshot &s = state.snapshots[i];
  if (!state.adjust.restoreEdits(s.edits, s.name)) {
    setStatus("Already looks like \"" + s.name + "\"", false);
    state.markDirty();
    return;
  }
  uiState.dropCurveSelections(); // curves and masks may both have changed
  updateImageFromAdjustments(); // same follow-up as undo() / redo(); clears the
                                // status
  setStatus("Restored snapshot: " + s.name, false);
}

inline void deleteSelectedSnapshot() {
  if (uiState.snapshots.sel < 0 || static_cast<size_t>(uiState.snapshots.sel) >= state.snapshots.size())
    return;
  const std::string name = state.snapshots[static_cast<size_t>(uiState.snapshots.sel)].name;
  state.snapshots.erase(state.snapshots.begin() + uiState.snapshots.sel);
  uiState.snapshots.sel = -1;
  state.snapshotsDirty = true;
  state.markDirty();
  setStatus("Deleted snapshot: " + name, false);
}

