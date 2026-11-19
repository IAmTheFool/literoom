// app/export.hpp: batch export from the filmstrip
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Batch export from the filmstrip ----------------
// File > Export Selected... / Export All Shown... write every picked picture to
// one folder with the Export pane's settings. Like Sync Settings, the pictures
// are not open, so the work is a pure function of each picture's FILE and its
// sidecar (exportPicture in workers/export_worker.hpp: decode, load the edits
// into a local AdjustmentSet, orient -> straighten -> crop -> render, resize,
// encode). The open picture is exported from its sidecar too, so its pending
// edits are flushed to disk first. Unlike Sync it can be stopped: Stop Export
// drops what is queued and abandons the picture being rendered. Export only
// READS sidecars, so it does not block opening pictures or editing; a picture
// is exported with the edits it has on disk when the batch reaches it.
static ExportWorker exportWorker;

struct ExportRun {
  bool active = false;
  size_t total = 0, ok = 0, bad = 0, cancelled = 0, shown = 0;
  std::string firstErr, folder;
};
static ExportRun exportRun;

static bool exportBusy() {
  if (!exportWorker.busy())
    return false;
  setStatus("Wait for the export to finish (or use Stop Export)", true);
  return true;
}

// The pictures of the strip in strip order, as the filter shows them:
// only the selected ones, or all of them.
static std::vector<std::string> exportTargets(bool selectedOnly) {
  std::vector<std::string> paths;
  const FilmStrip &f = state.film;
  for (const int i : f.view)
    if (i >= 0 && static_cast<size_t>(i) < f.files.size() &&
        (!selectedOnly || f.files[static_cast<size_t>(i)].selected))
      paths.push_back(f.files[static_cast<size_t>(i)].path);
  return paths;
}

static void startExportBatch(std::vector<std::string> paths) {
  if (state.dragging || exportBusy())
    return;
  if (paths.empty()) {
    setStatus(
        "Select pictures in the filmstrip first (Ctrl+click, Shift+click), "
        "or use Export All Shown",
        true);
    return;
  }
  auto dir = openFolderPicker("Export to Folder");
  if (!dir)
    return;       // user cancelled
  flushSidecar(); // the open picture's latest edits must be on disk
  ExportSettings s;
  s.jpeg = state.exportJpeg;
  s.png16 = !state.exportJpeg && state.exportPng16;
  s.quality = state.jpegQuality;
  s.keepMetadata = state.keepMetadata;
  s.removeGps = state.removeGps && state.keepMetadata;
  s.resize = state.exportResize;
  s.longEdge = state.exportLongEdge;
  s.overwrite = state.exportOverwrite;
  s.folder = *dir;
  const size_t n = paths.size();
  if (!exportWorker.start(std::move(paths), std::move(s)))
    return;
  exportRun = ExportRun{};
  exportRun.active = true;
  exportRun.total = n;
  exportRun.folder = *dir;
  setStatus("Exporting " + std::to_string(n) + " photo(s)...", false);
}

static void exportSelected() { startExportBatch(exportTargets(true)); }
static void exportAllShown() { startExportBatch(exportTargets(false)); }

static void stopExport() {
  if (exportWorker.busy()) {
    exportWorker.cancel();
    setStatus("Stopping the export...", false);
  }
}

// Polled once per frame from flushPendingAdjust.
static void collectExportJob() {
  if (!exportRun.active)
    return;
  // Ask BEFORE collecting: if the worker was already done, collect() below
  // returns the last outcomes and nothing more can arrive.
  const bool finished = !exportWorker.busy();
  for (const ExportOutcome &o : exportWorker.collect()) {
    if (o.ok)
      ++exportRun.ok;
    else if (o.cancelled)
      ++exportRun.cancelled;
    else if (++exportRun.bad == 1)
      exportRun.firstErr = pictureLabel(o.path) + ": " + o.error;
  }
  const size_t handled = exportRun.ok + exportRun.bad + exportRun.cancelled;
  if (!finished) {
    if (handled != exportRun.shown && exportRun.cancelled == 0) {
      exportRun.shown = handled;
      setStatus("Exporting " + std::to_string(handled) + " of " +
                    std::to_string(exportRun.total) + "...",
                false);
    }
    return;
  }
  exportRun.active = false;
  const std::string where = baseName(exportRun.folder);
  if (exportRun.cancelled > 0)
    setStatus("Export stopped: " + std::to_string(exportRun.ok) + " exported" +
                  (exportRun.bad
                       ? ", " + std::to_string(exportRun.bad) + " skipped"
                       : std::string()) +
                  ", " + std::to_string(exportRun.cancelled) + " not exported",
              exportRun.bad > 0);
  else if (exportRun.bad > 0)
    setStatus("Exported " + std::to_string(exportRun.ok) + ", skipped " +
                  std::to_string(exportRun.bad) + " (" + exportRun.firstErr +
                  ")",
              true);
  else
    setStatus("Exported " + std::to_string(exportRun.ok) + " photo(s) to " +
                  where,
              false);
}

