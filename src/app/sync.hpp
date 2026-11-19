// app/sync.hpp: Sync Settings: one look onto other pictures
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Sync Settings: one look onto other pictures ----------------
// Image > Sync Settings to Selected copies the open picture's look (sliders,
// curves, Color Mixer, Color Grading) onto the pictures picked in the filmstrip
// (Ctrl+click, Shift+click). The other pictures are not open, so there is no
// AdjustmentSet to edit: their sidecar files are rewritten directly by
// sidecarSyncLook, which keeps each picture's own crop, rotation and masks.
// A picture with no sidecar has to be decoded once to learn its size. All of
// that runs on a background worker. It is a LatestWorker but never cancelled:
// a second sync is refused while one runs, and opening pictures waits too, so
// no one reads a sidecar that is about to be replaced. The open picture is
// never in the batch (its edits live in memory and autosave).
//
// The same worker also does the **batch rating**: when pictures are selected,
// a rating / flag / color label command is applied to the open picture at once
// and to the other selected pictures here, as `metaOnly` jobs (see
// applyMetaPatch). That is a sidecarPatchMeta per picture instead of a
// sidecarSyncLook, with the same rules and the same refusals.
struct SyncJob {
  std::vector<std::string> paths;
  Preset look;           // only its values, curves, mix, grade and LUT are used
  bool metaOnly = false; // rate / flag / label the pictures instead
  MetaPatch patch;       // ... with this
};
struct SyncOutcome {
  std::string path;
  bool ok = false;
  std::string error; // why it was skipped
  PhotoMeta meta;    // metaOnly: the picture's meta after the patch
};
struct SyncResult {
  bool metaOnly = false;
  std::vector<SyncOutcome> items;
};

// 1 = text read, 0 = no sidecar, -1 = unreadable or too big. Any thread.
static int readSidecarText(const std::string &imagePath, std::string *text) {
  std::ifstream f(sidecarPath(imagePath), std::ios::binary | std::ios::ate);
  if (!f)
    return 0;
  const std::streamoff n = f.tellg();
  if (n <= 0 || n > (64 << 20))
    return -1;
  text->assign(static_cast<size_t>(n), '\0');
  f.seekg(0);
  f.read(text->data(), n);
  return f ? 1 : -1;
}

// Temp file, then rename, like saveSidecar. Any thread.
static bool writeTextAtomic(const std::filesystem::path &path,
                            const std::string &text, std::string *err) {
  std::filesystem::path tmp = path;
  tmp += ".tmp";
  std::error_code ec;
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    f.flush();
    if (!f) {
      f.close();
      std::filesystem::remove(tmp, ec);
      *err = "couldn't write its saved edits";
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::error_code ignore;
    std::filesystem::remove(tmp, ignore);
    *err = ec.message();
    return false;
  }
  return true;
}

// Worker thread only. Always returns a value (it can't be cancelled).
static std::optional<SyncResult> runSyncJob(SyncJob &&job, WorkerCancel) {
  SyncResult res;
  res.metaOnly = job.metaOnly;
  for (const std::string &path : job.paths) {
    SyncOutcome o;
    o.path = path;
    std::string old;
    int w = 0, h = 0;
    const int have = readSidecarText(path, &old);
    if (have == 0 && job.metaOnly &&
        job.patch.applyTo(PhotoMeta{})
            .isDefault()) { // nothing to record and no file to change: not
                            // worth a decode
      o.ok = true;
      res.items.push_back(std::move(o));
      continue;
    }
    if (have < 0)
      o.error = "couldn't read its saved edits";
    else {
      if (have == 0) { // no sidecar yet: record the picture's (upright) size
        std::string derr;
        if (!image_source::uprightSize(vcImageOf(path),
                                       exifOrientationOf(vcImageOf(path)), &w,
                                       &h, &derr))
          o.error = "couldn't open it: " + derr;
      }
      if (o.error.empty()) {
        std::string text;
        const bool built =
            job.metaOnly
                ? sidecarPatchMeta(old, baseName(vcImageOf(path)), w, h,
                                   job.patch, &text, &o.meta, &o.error)
                : sidecarSyncLook(old, baseName(vcImageOf(path)), w, h,
                                  job.look.values, job.look.curves,
                                  job.look.mix, job.look.grade, job.look.lut,
                                  &text, &o.error);
        if (built)
          o.ok = writeTextAtomic(sidecarPath(path), text, &o.error);
      }
    }
    res.items.push_back(std::move(o));
  }
  return res;
}

static LatestWorker<SyncJob, SyncResult> syncWorker(runSyncJob);

// True (with a status message) while a sync is running.
static bool syncBusy() {
  if (!syncWorker.outstanding())
    return false;
  setStatus("Wait for the sync to finish", true);
  return true;
}

// Polled once per frame from flushPendingAdjust.
static void collectSyncJob() {
  auto r = syncWorker.collect();
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
  syncWorker.submit(std::move(job));
  setStatus("Syncing settings to " + std::to_string(n) + " photo(s)...", false);
}

