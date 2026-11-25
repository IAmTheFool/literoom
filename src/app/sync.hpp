// app/sync.hpp: Sync Settings: the UI-thread glue
#pragma once

#include <cstddef> // size_t
#include <string>  // std::string, std::to_string
#include <utility> // std::move

#include "app/editor_state.hpp" // inline EditorState state, FilmStrip, setStatus
#include "app/filmstrip.hpp"    // filmApplyMeta, filmMarkEdited, filmViewPos
#include "app/saved_edits.hpp"  // pictureLabel
#include "app/sync_busy.hpp"    // syncBusy (new, see below)
#include "app/workers.hpp"      // workers()
#include "workers/sync_job.hpp" // SyncJob, SyncOutcome


// Polled once per frame from flushPendingAdjust.
inline void collectSyncJob() {
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
inline void syncSettings() {
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