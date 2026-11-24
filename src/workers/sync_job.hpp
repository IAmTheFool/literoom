// workers/sync_job.hpp - Sync Settings and batch rating: the background job.
//
// Moved out of app/sync.hpp. Image > Sync Settings to Selected copies the open
// picture's look (sliders, curves, Color Mixer, Color Grading) onto the pictures
// picked in the filmstrip (Ctrl+click, Shift+click). The other pictures are not
// open, so there is no AdjustmentSet to edit: their sidecar files are rewritten
// directly by sidecarSyncLook, which keeps each picture's own crop, rotation and
// masks. A picture with no sidecar has to be decoded once to learn its size. All
// of that runs on a background worker. It is a LatestWorker but never
// cancelled: a second sync is refused while one runs, and opening pictures
// waits too, so no one reads a sidecar that is about to be replaced. The open
// picture is never in the batch (its edits live in memory and autosave).
//
// The same worker also does the **batch rating**: when pictures are selected, a
// rating / flag / color label command is applied to the open picture at once
// and to the other selected pictures here, as `metaOnly` jobs (see
// applyMetaPatch). That is a sidecarPatchMeta per picture instead of a
// sidecarSyncLook, with the same rules and the same refusals.
//
// This part is pure (no `state`). The one SyncWorker lives in `Workers`
// (app/workers.hpp); the UI-thread glue (syncBusy, collectSyncJob,
// syncSettings) stays in app/sync.hpp.
//
// Failure handling.
//  * A sidecar that EXISTS but can't be opened (locked, no permission) is
//    "unreadable", never "absent". Treating it as absent would rebuild the
//    sidecar from nothing and rename it over the real file, losing that
//    picture's crop, rotation and masks. The picture is skipped with a reason.
//  * One job is a whole batch, and every picture the loop has finished is
//    already written to disk. An exception on picture 7 of 20 must not throw
//    away the outcomes of 1 to 6 (the UI would never mark them as edited), so
//    each picture is guarded on its own: it becomes a skipped picture with a
//    reason, and the batch carries on.
#pragma once
#include <exception>
#include <filesystem>
#include <fstream>
#include <new>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "io/file_utils.hpp"   // baseName
#include "io/image_meta.hpp"   // exifOrientationOf
#include "io/image_source.hpp" // uprightSize
#include "io/photo_meta.hpp"   // PhotoMeta
#include "io/preset.hpp"       // Preset
#include "io/sidecar.hpp"      // sidecarSyncLook, sidecarPatchMeta, MetaPatch
#include "io/sidecar_file.hpp" // readSidecarFile, sidecarFilePath
#include "io/virtual_copy.hpp" // vcImageOf
#include "workers/latest_worker.hpp"

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

// Temp file, then rename, like saveSidecar. Any thread.
inline bool writeTextAtomic(const std::filesystem::path &path,
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
inline SyncOutcome syncOne(SyncJob &job, const std::string &path) {
  SyncOutcome o;
  o.path = path;
  std::string old, why;
  int w = 0, h = 0;
  // Absent / Ok are the only states we can rewrite from. Anything else (an
  // unreadable, empty or oversized file) is skipped, never treated as absent.
  const SidecarRead have = readSidecarFile(path, &old, &why);
  if (have == SidecarRead::Absent && job.metaOnly &&
      job.patch.applyTo(PhotoMeta{})
          .isDefault()) { // nothing to record and no file to change: not
                          // worth a decode
    o.ok = true;
    return o;
  }
  if (have != SidecarRead::Absent && have != SidecarRead::Ok) {
    o.error = have == SidecarRead::Empty
                  ? "its saved edits file is empty"
                  : "couldn't read its saved edits" +
                        (why.empty() ? std::string() : " (" + why + ")");
    return o;
  }
  if (have == SidecarRead::Absent) { // no sidecar yet: record the picture's
                                     // (upright) size
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
    o.ok = writeTextAtomic(sidecarFilePath(path), text, &o.error);
  return o;
}

// Worker thread only. Always returns a value (it can't be cancelled).
inline std::optional<SyncResult> runSyncJob(SyncJob &&job, WorkerCancel) {
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

using SyncWorker = LatestWorker<SyncJob, SyncResult>;
