// app/sync.hpp: Sync Settings: one look onto other pictures
//
// UI-shell code split out of main.cpp. A plain move, apart from the two
// failure fixes described under "Failure handling" below.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once
#include <exception>
#include <new>

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
//
// Failure handling.
//  * A sidecar that EXISTS but can't be opened (locked, no permission) is
//    "unreadable", never "absent".
//    Treating it as absent would rebuild the sidecar from nothing and rename
//    it over the real file, losing that picture's crop, rotation and masks.
//    The picture is skipped with a reason instead.
//  * One job is a whole batch, and every picture the loop has finished is
//    already written to disk. An exception on picture 7 of 20 must not throw
//    away the outcomes of 1 to 6 (the UI would never mark them as edited), so
//    each picture is guarded on its own: it becomes a skipped picture with a
//    reason, and the batch carries on.
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

// Largest sidecar a sync will read: the same limit the editor puts on one.
constexpr std::streamoff kSyncMaxSidecarBytes = std::streamoff{64} << 20;

// 1 = text read, 0 = no sidecar, -1 = unreadable or too big. Any thread.
// "No sidecar" means the file is not there; a file that is there but can't be
// opened is -1, never 0 (see "Failure handling").
static int readSidecarText(const std::string &imagePath, std::string *text) {
  const std::filesystem::path p = sidecarPath(imagePath);
  std::error_code ec;
  const bool exists = std::filesystem::exists(p, ec);
  if (ec)
    return -1; // can't even tell: don't guess "absent"
  if (!exists)
    return 0;
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f)
    return -1; // there, but can't be opened
  const std::streamoff n = f.tellg();
  if (n <= 0 || n > kSyncMaxSidecarBytes)
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

// One picture of a batch. Worker thread only. May throw (std::bad_alloc, a
// library exception): runSyncJob is the boundary that turns that into a skipped
// picture.
static SyncOutcome syncOne(SyncJob &job, const std::string &path) {
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
    return o;
  }
  if (have < 0) {
    o.error = "couldn't read its saved edits";
    return o;
  }
  if (have == 0) { // no sidecar yet: record the picture's (upright) size
    std::string derr;
    if (!image_source::uprightSize(vcImageOf(path),
                                   exifOrientationOf(vcImageOf(path)), &w, &h,
                                   &derr)) {
      o.error = "couldn't open it: " + derr;
      return o;
    }
  }
  std::string text;
  const bool built =
      job.metaOnly
          ? sidecarPatchMeta(old, baseName(vcImageOf(path)), w, h, job.patch,
                             &text, &o.meta, &o.error)
          : sidecarSyncLook(old, baseName(vcImageOf(path)), w, h,
                            job.look.values, job.look.curves, job.look.mix,
                            job.look.grade, job.look.lut, &text, &o.error);
  if (built)
    o.ok = writeTextAtomic(sidecarPath(path), text, &o.error);
  return o;
}

// Worker thread only. Always returns a value (it can't be cancelled).
static std::optional<SyncResult> runSyncJob(SyncJob &&job, WorkerCancel) {
  SyncResult res;
  res.metaOnly = job.metaOnly;
  for (const std::string &path : job.paths) {
    SyncOutcome o;
    try {
      o = syncOne(job, path);
    } catch (const std::bad_alloc &) {
      o = SyncOutcome{};
      o.path = path;
      o.error = "ran out of memory";
    } catch (const std::exception &e) {
      o = SyncOutcome{};
      o.path = path;
      o.error = e.what();
      if (o.error.empty())
        o.error = "unexpected error";
    } catch (...) {
      o = SyncOutcome{};
      o.path = path;
      o.error = "unexpected error";
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