// app/sync.hpp: Sync Settings: the UI-thread glue
//
// Image > Sync Settings to Selected copies the open picture's look onto the
// pictures picked in the filmstrip; the same worker also does the batch rating.
// The job (SyncJob, SyncOutcome, syncOne, runSyncJob) is pure and lives in
// workers/sync_job.hpp, which also explains how a sync works and how it fails;
// the worker object is workers().sync (app/workers.hpp). What is left here is
// what touches `state` and the status bar.
//
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// True (with a status message) while a sync is running.
static bool syncBusy() {
  if (!workers().sync.outstanding())
    return false;
  setStatus("Wait for the sync to finish", true);
  return true;
}

// Polled once per frame from flushPendingAdjust.
static void collectSyncJob() {
  auto r = workers().sync.collect();
  if (!r)
    return;
  size_t ok = 0, bad = 0;
  std::string firstErr;
  for (const SyncOutcome &o : r->items) {
    if (o.ok) {
      ++ok;
      if (r->metaOnly)
        filmApplyMeta(o.path, o.meta); // a rating alone sets no "has edits" dot
      else
        filmMarkEdited(o.path); // the strip's "has edits" dot
    } else if (++bad == 1)
      firstErr = pictureLabel(o.path) + ": " + o.error;
  }
  if (r->metaOnly) {
    if (bad == 0)
      setStatus("Updated " + std::to_string(ok) + " selected photo(s)", false);
    else
      setStatus("Updated " + std::to_string(ok) + ", skipped " +
                    std::to_string(bad) + " (" + firstErr + ")",
                true);
    return;
  }
  if (bad == 0)
    setStatus("Synced settings to " + std::to_string(ok) + " photo(s)", false);
  else
    setStatus("Synced " + std::to_string(ok) + ", skipped " +
                  std::to_string(bad) + " (" + firstErr + ")",
              true);
}

// Image > Sync Settings to Selected.
static void syncSettings() {
  if (state.dragging || !state.original || syncBusy())
    return;
  const FilmStrip &f = state.film;
  SyncJob job;
  for (size_t i = 0; i < f.files.size(); ++i)
    if (f.files[i].selected && static_cast<int>(i) != f.current &&
        filmViewPos(static_cast<int>(i)) >= 0) // never one the filter hides
      job.paths.push_back(f.files[i].path);
  if (job.paths.empty()) {
    setStatus(
        "Select other photos in the filmstrip first (Ctrl+click, Shift+click)",
        true);
    return;
  }
  job.look = state.adjust.currentPreset(std::string());
  const size_t n = job.paths.size();
  workers().sync.submit(std::move(job));
  setStatus("Syncing settings to " + std::to_string(n) + " photo(s)...", false);
}