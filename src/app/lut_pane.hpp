// app/lut_pane.hpp: 3D LUT list, previews and cache
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "liteui.hpp" // CanvasImage

#include "color/lut3d.hpp"      // LutRef
#include "core/adjustments.hpp" // AdjId
#include "io/lut_library.hpp" // LutEntry, LutStatus, LutResolved, clearLutCache,
// lutDir, listLutEntries, acquireLut, importLutFile
#include "io/minijson.hpp"     // mj::Json
#include "io/photo_meta.hpp"   // PhotoMeta
#include "io/sidecar.hpp"      // SidecarHead, SidecarCheck, sidecarCheckText,
                               // sidecarMetaFromRoot, sidecarSnapshotsFromJson,
                               // sidecarNeutralEditsDump
#include "io/sidecar_file.hpp" // SidecarRead, readSidecarFile
#include "workers/lut_preview_worker.hpp" // LutPreviewJob, LutPreviewResult

#include "app/constants.hpp"    // kLutRows, kLutsActivity
#include "app/editor_state.hpp" // state, setStatus
#include "app/saved_edits.hpp"  // sidecarPath
#include "app/workers.hpp"      // workers().lutPreview

// ---------------- 3D LUTs ----------------
// The LUTs pane (see "LUTs" in main): the library's .cube files as a list, each
// with a preview of the open picture under that LUT. The picture's own LUT is
// part of its edits (AdjustmentSet::lut(), with LUT Amount as its strength);
// everything here is the UI around it. The library and its cache are
// io/lut_library.hpp, the previews are workers/lut_preview_worker.hpp.
struct LutRow {
  LutEntry entry; // name + path (hash and size arrive with the preview)
  std::shared_ptr<const CanvasImage>
      thumb;                 // null until made, or with no picture open
  std::string error;         // why the file can't be used; "" if it can
  std::string warning;       // "may expect camera-log input"; "" if not
  uint64_t paintVersion = 0; // changes whenever what the row shows changes
};
inline std::vector<LutRow> gLuts;
inline bool gLutsLoaded =
    false; // the folder was listed since the pane was last opened
inline uint64_t gLutPaintCounter = 0;
inline uint64_t gLutSerial = 0; // the job whose results are taken in

// What the previews were made for: the picture's pixels and the edit state. A
// change of either makes them stale. The LUTs pane's own changes (choosing a
// LUT, its amount) do not, because a preview shows every LUT on the picture
// WITHOUT the one it has now (see lutPreviewsFollow).
struct LutBuilt {
  bool valid = false;
  const CanvasImage *source = nullptr;
  uint64_t version = 0;
};
inline LutBuilt gLutBuilt;
inline uint64_t gLutCacheEpoch =
    1; // bumped when the LUT cache is emptied (see currentLutStatus)

// Empties the library's cache: a LUT file may have changed (an import, another
// picture).
inline void dropLutCache() {
  clearLutCache();
  ++gLutCacheEpoch;
}

// Lists the library folder afresh. Rows keep what is known about a file that is
// still there (its preview, size, warning), and the previews are only made
// again when the list itself changed.
inline void refreshLuts() {
  std::vector<std::string>
      oldPaths; // taken before the rows are moved from below
  for (const LutRow &o : gLuts)
    oldPaths.push_back(o.entry.path);
  std::vector<LutRow> old = std::move(gLuts);
  gLuts.clear();
  for (LutEntry &e : listLutEntries(lutDir())) {
    if (gLuts.size() >= kLutRows)
      break;
    LutRow r;
    r.entry = std::move(e);
    r.paintVersion = ++gLutPaintCounter;
    for (LutRow &o : old)
      if (o.entry.path == r.entry.path) {
        r = std::move(o); // same file: keep its preview, hash and size (and its
                          // paint version)
        break;
      }
    gLuts.push_back(std::move(r));
  }
  bool same = oldPaths.size() == gLuts.size();
  for (size_t i = 0; same && i < gLuts.size(); ++i)
    same = gLuts[i].entry.path == oldPaths[i];
  gLutsLoaded = true;
  if (!same) { // a file came or went (an import, or one removed by hand): make
               // the previews again
    workers().lutPreview.cancel();
    gLutBuilt.valid = false;
  }
  state.markDirty();
}

// Where the open picture's LUT stands in the library. Looked up again only when
// the edit state or the cache changed (the pane's text asks every frame).
inline LutStatus currentLutStatus() {
  static uint64_t forVersion = 0, forEpoch = 0;
  static LutStatus cached = LutStatus::None;
  if (forVersion != state.adjust.version() || forEpoch != gLutCacheEpoch) {
    cached = state.adjust.lutStatus();
    forVersion = state.adjust.version();
    forEpoch = gLutCacheEpoch;
  }
  return cached;
}

// Says so in the status bar when the picture's LUT can't be used as it was
// chosen: gone from the library, damaged, or a file that has changed since.
// True if it said something.
inline bool noteLutProblem() {
  const std::string name = "\"" + state.adjust.lut().name + "\"";
  switch (currentLutStatus()) {
  case LutStatus::Missing:
    setStatus("LUT " + name +
                  " is not in the LUT library: the picture shows without it",
              true);
    return true;
  case LutStatus::Unreadable:
    setStatus("LUT " + name + " can't be read: the picture shows without it",
              true);
    return true;
  case LutStatus::HashChanged:
    setStatus("LUT " + name + " has changed since it was chosen", false);
    return true;
  default:
    return false;
  }
}

// "line 3: ..." out of "file.cube: line 3: ..." (the row already names the
// file).
inline std::string lutReason(const std::string &error) {
  const size_t k = error.find(": ");
  return k == std::string::npos ? error : error.substr(k + 2);
}

// The previews match the picture and its edits as they are right now.
inline bool lutPreviewsCurrent() {
  return gLutBuilt.valid && gLutBuilt.source == state.source.get() &&
         gLutBuilt.version == state.adjust.version();
}
// Call after a change made by the LUTs pane itself (`wasCurrent` =
// lutPreviewsCurrent() just before it): the previews do not depend on it, so
// they stay current. An unrelated edit made in between keeps them stale.
inline void lutPreviewsFollow(bool wasCurrent) {
  if (wasCurrent)
    gLutBuilt.version = state.adjust.version();
}

// The LUT Amount slider's setter. The previews show every LUT at 100 %,
// whatever the amount is, so moving it keeps them current.
inline void setLutAmount(float v) {
  const bool current = lutPreviewsCurrent();
  state.adjust.set(AdjId::LutAmount, v);
  lutPreviewsFollow(current);
}

// Once per frame (flushPendingAdjust): takes in the previews the worker has
// made, and, with the pane open, starts a new batch when the list or the
// picture's look has changed since the last.
inline void pollLutPreviews() {
  for (LutPreviewResult &r : workers().lutPreview.collect()) {
    if (r.serial != gLutSerial)
      continue; // a batch that was replaced
    for (LutRow &row : gLuts)
      if (row.entry.path == r.path) {
        row.entry.hash = r.hash;
        row.entry.size = r.size;
        row.thumb = std::move(r.thumb);
        row.error = std::move(r.error);
        row.warning = std::move(r.warning);
        row.paintVersion = ++gLutPaintCounter;
        state.markDirty();
        break;
      }
  }
  if (state.activeActivity != kLutsActivity)
    return;
  if (!gLutsLoaded)
    refreshLuts();
  if (!state.original || !state.source || state.dragging || gLuts.empty())
    return; // no picture: the list is just names
  if (lutPreviewsCurrent())
    return;
  if (gLutBuilt.valid && gLutBuilt.source != state.source.get())
    for (LutRow &row : gLuts) { // another picture: what is shown is wrong until
                                // the new previews arrive
      row.thumb.reset();
      row.paintVersion = ++gLutPaintCounter;
    }
  LutPreviewJob job;
  job.serial = ++gLutSerial;
  job.base = state.previewBase ? state.previewBase : state.source;
  job.params = state.adjust.buildRenderParams();
  for (const LutRow &row : gLuts)
    job.items.push_back(row.entry);
  workers().lutPreview.submit(std::move(job));
  gLutBuilt = LutBuilt{true, state.source.get(), state.adjust.version()};
  state.markDirty();
}

// Gives the picture the LUT of list row i: one undo step, "LUT: <name>" in
// History.
inline void applyLutRow(size_t i) {
  if (state.dragging || !state.original || i >= gLuts.size())
    return;
  const LutRow &row = gLuts[i];
  if (!row.error.empty()) {
    setStatus("Can't use " + row.entry.name + ": " + lutReason(row.error),
              true);
    return;
  }
  // The hash that goes into the edit is the file's own, as the library reads it
  // now.
  const LutResolved res = acquireLut(LutRef{row.entry.name, 0});
  if (!res.lut) {
    setStatus("Couldn't read the LUT " + row.entry.name, true);
    return;
  }
  const bool current = lutPreviewsCurrent();
  if (!state.adjust.setLut(LutRef{row.entry.name, res.hash})) {
    setStatus("Already using the LUT " + row.entry.name, false);
    return;
  }
  lutPreviewsFollow(current);
  state.markAdjustDirty(); // re-render through the normal (drag-safe) path
  state.markDirty();
  setStatus("Applied LUT: " + row.entry.name, false);
}

// Takes the LUT off the picture (and its amount back to 0): one undo step,
// "Remove LUT".
inline void removeLut() {
  if (state.dragging || !state.original)
    return;
  const bool current = lutPreviewsCurrent();
  if (!state.adjust.setLut(LutRef{}))
    return;
  lutPreviewsFollow(current);
  state.markAdjustDirty();
  state.markDirty();
  setStatus("Removed the LUT", false);
}

// Copies .cube files into the library (see importLutFile) and re-lists it; says
// what happened in the status bar.
inline void importLutPaths(const std::vector<std::string> &files) {
  if (files.empty())
    return;
  size_t ok = 0, bad = 0;
  std::string lastName, firstError, logName;
  for (const std::string &f : files) {
    LutEntry e;
    std::string warning, err;
    if (importLutFile(f, lutDir(), &e, &warning, &err)) {
      ++ok;
      lastName = e.name;
      if (!warning.empty() && logName.empty())
        logName = e.name;
    } else {
      ++bad;
      if (firstError.empty())
        firstError = err;
    }
  }
  dropLutCache();
  refreshLuts();
  if (ok == 0) {
    setStatus("Couldn't import: " + firstError, true);
    return;
  }
  if (!logName.empty()) // not a refusal: it is imported, it may just look wrong
                        // on this kind of picture
  {
    setStatus(logName +
                  " may expect camera-log input and look wrong on this picture",
              true);
    return;
  }
  std::string msg = ok == 1 ? "Imported LUT: " + lastName
                            : "Imported " + std::to_string(ok) + " LUTs";
  if (bad > 0)
    msg += " (" + std::to_string(bad) + " skipped: " + firstError + ")";
  setStatus(msg, bad > 0);
}

inline void importLuts() {
  importLutPaths(openFilesPicker(
      "Import LUTs", {{"3D LUTs", "*.cube"}, {"All Files", "*.*"}}));
}

// The line under a row's name: the table size, or why the file can't be used.
inline std::string lutRowDetail(size_t i) {
  if (i >= gLuts.size())
    return std::string();
  const LutRow &r = gLuts[i];
  if (!r.error.empty())
    return "Can't be read: " + lutReason(r.error);
  std::string s;
  if (r.entry.size > 0)
    s = std::to_string(r.entry.size) + "\u00B3 points";
  if (!r.warning.empty())
    s += std::string(s.empty() ? "" : "  \u2022  ") +
         "may expect camera-log input";
  return s;
}

// What the pane says about the picture's own LUT.
inline std::string lutCurrentText() {
  const LutRef &r = state.adjust.lut();
  if (r.empty())
    return "No LUT on this picture";
  switch (currentLutStatus()) {
  case LutStatus::Missing:
    return "LUT \"" + r.name + "\" is not in the library, so it has no effect";
  case LutStatus::Unreadable:
    return "LUT \"" + r.name + "\" can't be read, so it has no effect";
  case LutStatus::HashChanged:
    return "LUT: " + r.name + " (the file has changed since it was chosen)";
  default:
    return "LUT: " + r.name;
  }
}

// Keeps a copy of a saved-edits file that is about to be ignored, as
// "<sidecar>.bak", so that the edits the next autosave replaces it with can't
// destroy what was in it. An existing .bak is left alone (the first copy is the
// one worth keeping). True if the copy exists afterwards.
inline bool backUpIgnoredSidecar(const std::filesystem::path &sidecar) {
  std::filesystem::path bak = sidecar;
  bak += ".bak";
  std::error_code ec;
  std::filesystem::copy_file(sidecar, bak,
                             std::filesystem::copy_options::skip_existing, ec);
  return !ec;
}

// Applies the picture's saved edits, if it has any. Call right after
// adjust.reset() + setOriginalSize() and BEFORE syncSource(), so the saved
// crop / rotation is what gets built.
//
// A saved-edits file this function can't use must not be lost to the next
// autosave, which would write the picture's (empty) edits over it. Two kinds:
//  * UNREADABLE: the file is there but can't be opened, read or checked
//    (locked by another program, no permission, too big). Its content is
//    unknown and can't be copied either, so the picture is opened with
//    state.sidecarBlocked set: nothing may write its sidecar until the picture
//    is opened again (see the notes in saved_edits.hpp).
//  * UNUSABLE: the file was read but can't be applied (damaged, written by a
//    newer Literoom, made for a picture of another size, or its edits don't
//    parse). It is copied to "<sidecar>.bak" first and the picture opens clean,
//    as before. Only if even that copy fails does it count as unreadable.
inline void loadSidecar() {
  state.sidecarSaved = state.adjust.version(); // pristine unless loaded below
  state.meta = PhotoMeta{}; // unrated unless the file says so
  state.metaDirty = false;
  state.snapshots.clear(); // none unless the file has some
  state.snapshotsDirty = false;
  state.sidecarBlocked = false; // set below if the file can't be touched safely
  dropLutCache(); // a LUT file may have changed since the last picture was open

  const std::filesystem::path sidecar = sidecarPath(state.imagePath);

  // setStatus cuts a message at 110 characters, so the part that matters
  // comes first.
  const auto blocked = [&](const std::string &why) {
    state.sidecarBlocked = true;
    setStatus("Changes won't be saved: saved edits unreadable (" + why + ")",
              true);
  };
  const auto unusable = [&](const std::string &why) {
    if (backUpIgnoredSidecar(sidecar))
      setStatus("Ignored saved edits: " + why + " (old file kept as .bak)",
                true);
    else
      blocked("can't back up the old file");
  };

  std::string text, why;
  switch (readSidecarFile(state.imagePath, &text, &why)) {
  case SidecarRead::Absent:
    return;                // no saved edits: the normal case
  case SidecarRead::Empty: // an empty file holds nothing to lose
    setStatus("Ignored saved edits: the file is empty", true);
    return;
  case SidecarRead::TooBig:
  case SidecarRead::Unreadable:
    blocked(why);
    return;
  case SidecarRead::Ok:
    break;
  }
  // The reader has closed the file, so the backup below can copy it.
  SidecarHead head;
  switch (sidecarCheckText(text, state.original->width, state.original->height,
                           &head)) {
  case SidecarCheck::Ok:
    break;
  case SidecarCheck::Unparsable:
    unusable(head.parseError.empty() ? std::string("unreadable")
                                     : head.parseError);
    return;
  case SidecarCheck::Newer:
    unusable("made by a newer Literoom");
    return;
  case SidecarCheck::NoSize:
  case SidecarCheck::SizeChanged:
    unusable("the picture's size has changed");
    return;
  }
  const mj::Json &root = head.root;
  const mj::Json *edits = root.find("edits");
  if (!edits || !state.adjust.loadJson(*edits)) {
    unusable("unreadable");
    return;
  }
  state.sidecarSaved = state.adjust.version();
  state.meta = sidecarMetaFromRoot(root);
  state.snapshots = sidecarSnapshotsFromJson(
      root.find("snapshots"), state.original->width, state.original->height);
  if (edits->dump() !=
      sidecarNeutralEditsDump()) // a rating alone isn't "edits"
    setStatus("Restored saved edits", false);
  noteLutProblem(); // a LUT the library no longer has (or has changed) is worth
                    // a word
}