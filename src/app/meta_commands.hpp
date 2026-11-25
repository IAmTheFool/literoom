#pragma once

#include <algorithm> // std::clamp, std::remove_if, std::sort
#include <cstddef>   // size_t
#include <memory>    // std::shared_ptr, std::make_shared
#include <optional>  // std::optional
#include <string>    // std::string, std::to_string
#include <utility>   // std::move
#include <vector>    // std::vector

#include "liteui.hpp" // openFilesPicker, saveFilePicker,
                      // liteui_image::jpegSizeOk, kJpegMaxSide,
                      // encodePng (?)

#include "app/constants.hpp"       // kNoActivity (?), kInfoActivity (?)
#include "app/crop.hpp"            // syncSource
#include "app/editor_state.hpp"    // state, FilmStrip, FilmEntry, setStatus,
                                   // clearStatus, applyZoomDev
#include "app/filmstrip.hpp"       // filmRebuildView, filmViewPos, filmLeave,
                                   // filmSync, filmAdoptFiles, filmQueueThumbs,
                                   // filmOpenIndex, starsText, kLabelDisplay
#include "app/geometry_worker.hpp" // finishGeometryJob
#include "app/lut_pane.hpp"        // loadSidecar
#include "app/render_update.hpp" // updateImageFromAdjustments, finishFullRender
#include "app/saved_edits.hpp"   // pictureLabel, flushSidecar, announceColorTag
#include "app/sync.hpp"          // syncBusy
#include "app/ui_state.hpp"      // uiState
#include "app/workers.hpp"       // workers()
#include "core/geometry.hpp"     // Dims
#include "core/image16_convert.hpp" // toImageBuffer, exportPngOptions
#include "core/raster_ops.hpp"      // fitLongEdge, resizeToLongEdge
#include "io/color_tag.hpp"         // color_tag::colorTagOf, carryColorToPng,
                                    // dropEncoderColor
#include "io/file_utils.hpp" // baseName, fileExtLower, isJpegPath, naturalLess
#include "io/photo_meta.hpp" // PhotoMeta, PhotoFlag, MetaPatch, kMaxRating,
                             // kLabelCount, photoMetaSanitized
#include "io/virtual_copy.hpp"       // vcImageOf, vcCopyOf
#include "render/render_core.hpp"    // renderToImage16
#include "workers/decode_job.hpp"    // DecodeWorker
#include "workers/export_worker.hpp" // saveJpegFile
#include "workers/sync_job.hpp"      // SyncJob

// ---------------- Rating, flag and color label (open picture + selection)
// ---------------- Not edits: no undo step, no History label, adjust.version()
// is not touched. The open picture's value lives in state.meta, is mirrored
// into the strip's entry, and is saved in the sidecar's "meta" object
// (autosaveSidecar / flushSidecar look at state.metaDirty).
//
// A command (applyMetaPatch) changes the open picture at once. If other
// pictures are selected in the filmstrip it changes them too, on the Sync
// Settings worker: their sidecars are patched in the background
// (sidecarPatchMeta) and the strip updates when the batch finishes. The open
// picture is never in that batch, so it can't race with autosave. "Choose the
// value it already has to clear it" is decided from the open picture, and the
// result (set or clear) is then applied to every selected picture alike.
inline void setPhotoMeta(const PhotoMeta &in, const std::string &message) {
  const PhotoMeta m = photoMetaSanitized(in);
  if (m == state.meta)
    return;
  state.meta = m;
  state.metaDirty = true;
  FilmStrip &f = state.film;
  if (f.current >= 0 && static_cast<size_t>(f.current) < f.files.size()) {
    FilmEntry &e = f.files[static_cast<size_t>(f.current)];
    e.meta = m;
    e.scanned = true;
  }
  if (f.filter.active())
    filmRebuildView(); // the open picture stays in; others may come or go
  f.dirty = true;
  setStatus(message, false);
}

// The selected pictures other than the open one that the strip shows.
inline std::vector<std::string> metaBatchTargets() {
  const FilmStrip &f = state.film;
  std::vector<std::string> paths;
  for (size_t i = 0; i < f.files.size(); ++i)
    if (f.files[i].selected && static_cast<int>(i) != f.current &&
        filmViewPos(static_cast<int>(i)) >= 0) // never one the filter hides
      paths.push_back(f.files[i].path);
  return paths;
}

inline void applyMetaPatch(const MetaPatch &patch, const std::string &message) {
  if (!state.original || state.imagePath.empty()) {
    setStatus("Open a picture first", true);
    return;
  }
  std::vector<std::string> others = metaBatchTargets();
  if (!others.empty() && syncBusy()) // one batch at a time; nothing changes yet
    return;
  setPhotoMeta(patch.applyTo(state.meta), message);
  if (others.empty())
    return;
  SyncJob job;
  job.paths = std::move(others);
  job.metaOnly = true;
  job.patch = patch;
  const size_t n = job.paths.size();
  workers().sync.submit(std::move(job));
  setStatus(message + " \u2014 updating " + std::to_string(n) +
                " selected photo(s)...",
            false);
}

inline void ratePhoto(int n) {
  n = std::clamp(n, 0, kMaxRating);
  const int v = n == state.meta.rating ? 0 : n;
  MetaPatch p;
  p.rating = v;
  applyMetaPatch(p, v > 0 ? "Rating: " + starsText(v)
                          : std::string("Rating cleared"));
}

// Pick or Reject toggles; PhotoFlag::None clears.
inline void flagPhoto(PhotoFlag flag) {
  const PhotoFlag v = flag == state.meta.flag ? PhotoFlag::None : flag;
  MetaPatch p;
  p.flag = v;
  applyMetaPatch(p, v == PhotoFlag::Pick     ? "Flagged as pick"
                    : v == PhotoFlag::Reject ? "Flagged as rejected"
                                             : "Flag cleared");
}

// 1..kLabelCount toggles that color; 0 clears.
inline void labelPhoto(int c) {
  c = std::clamp(c, 0, kLabelCount);
  const int v = c == state.meta.label ? 0 : c;
  MetaPatch p;
  p.label = v;
  applyMetaPatch(p, v > 0 ? std::string("Label: ") + kLabelDisplay[v - 1]
                          : std::string("Label cleared"));
}

inline void adoptDecoded(DecodeWorker::Result &&r) {
  if (!r.image) {
    // Leave whatever image is already open untouched and say what happened.
    setStatus("Couldn't open " + pictureLabel(r.path) + ": " +
                  (r.error.empty() ? std::string("unsupported or damaged image")
                                   : r.error),
              true);
    state.film.target = state.film.current; // stay on the picture still open
    state.film.dirty = true;
    state.markDirty();
    return;
  }
  state.wbPicking = false;      // a new picture: the picker must be armed again
  uiState.resetForNewPicture(); // selections of the previous picture
  clearStatus();
  flushSidecar(); // the picture being replaced keeps its latest edits
  filmLeave();    // ...and its filmstrip thumbnail shows them

  // Anything still in flight belongs to the previous picture; its result
  // must never be adopted over the new one.
  workers().render.invalidate();
  workers().geometry.invalidate();
  workers().segment.invalidate();
  state.mask.objPending = {};
  ++state.imageSerial;

  state.original = std::make_shared<CanvasImage>(std::move(*r.image));
  state.original16 =
      std::move(r.wide); // null for an 8-bit file; syncSource(true) below
                         // rebuilds the twin chain
  state.imagePath = std::move(r.path);
  applyZoomDev(1.0f); // start fresh at 100% on every newly opened image
  state.view.scrollX = state.view.scrollY = 0.0f; // centered
  state.adjust.reset(); // and with no adjustment, crop or rotation applied yet
  state.adjust.setOriginalSize(state.original->width, state.original->height);
  state.maskSel = -1;
  state.mask.part = 0;
  loadSidecar(); // this picture's saved edits, if any (before syncSource: crop
                 // / rotation)
  filmSync(state.imagePath); // the strip follows: folder, selection, thumbnails
  state.dragging = false;
  state.dragIsAngle = false;
  state.image.reset(); // the old file's pixels: force the synchronous path
  syncSource(true); // source = original; builds previewBase, resets selection
  updateImageFromAdjustments();    // builds state.image from state.source
  state.colorNoticePending = true; // a large picture's geometry job would clear
                                   // the notice when it lands
  announceColorTag();
}

// Non-blocking; polled once per frame from flushPendingAdjust.
inline void collectDecodeJob() {
  if (auto r = workers().decode.collect())
    adoptDecoded(std::move(*r));
}

inline void openImage() {
  if (syncBusy())
    return;
  auto paths = openFilesPicker(
      "Open Images", {{"Images", "*.png;*.jpg;*.jpeg"}, {"All Files", "*.*"}});
  if (paths.empty())
    return; // user cancelled

  if (paths.size() > 1) {
    // The dialog also offers "All Files": keep only pictures we can show.
    paths.erase(std::remove_if(paths.begin(), paths.end(),
                               [](const std::string &p) {
                                 const std::string e = fileExtLower(p);
                                 return e != "jpg" && e != "jpeg" && e != "png";
                               }),
                paths.end());
    if (paths.empty()) {
      setStatus("No JPEG or PNG pictures selected", true);
      return;
    }
    std::sort(paths.begin(), paths.end(),
              [](const std::string &a, const std::string &b) {
                const std::string x = baseName(a), y = baseName(b);
                return naturalLess(x, y) || (!naturalLess(y, x) && x < y);
              });
  }

  filmAdoptFiles(paths); // the strip is exactly what was picked
  filmQueueThumbs();
  filmOpenIndex(0); // same background decode as before
}

// saveJpegFile() lives in workers/export_worker.hpp (shared with batch export).

// Exports the picture as you see it (every edit, crop and rotation applied).
// The format follows the file name the user picks (.jpg / .jpeg / .png); the
// Export pane's format choice only decides what the dialog offers first.

inline void exportImage() {
  if (!state.image)
    return;            // nothing open to export
  finishGeometryJob(); // never export the stale (pre-rebuild) picture
  finishFullRender();  // never export the stale frame of a pending render

  std::string stem = baseName(vcImageOf(state.imagePath));
  if (const auto dot = stem.find_last_of('.');
      dot != std::string::npos && dot > 0)
    stem.resize(dot);
  if (stem.empty())
    stem = "export";
  if (const int n = vcCopyOf(state.imagePath); n > 0)
    stem += "-copy" + std::to_string(
                          n); // keeps a copy's export apart from the original's

  const bool wantJpeg = state.exportJpeg;
  auto path =
      wantJpeg
          ? saveFilePicker(
                "Export as JPEG", stem + "-edit.jpg",
                {{"JPEG Image", "*.jpg;*.jpeg"}, {"PNG Image", "*.png"}}, "jpg")
          : saveFilePicker(
                state.exportPng16 ? "Export as PNG (16-bit)" : "Export as PNG",
                stem + "-edit.png",
                {{"PNG Image", "*.png"}, {"JPEG Image", "*.jpg;*.jpeg"}},
                "png");
  if (!path)
    return; // user cancelled

  std::string out = *path;
  bool jpeg = isJpegPath(out);
  if (!jpeg &&
      fileExtLower(out) != "png") { // no usable extension: use the pane's
                                    // format and name the file for it
    jpeg = wantJpeg;
    out += jpeg ? ".jpg" : ".png";
  }
  // The file name decides the format; 16 bits only means something for PNG.
  const bool png16 = !jpeg && state.exportPng16;

  // The size that will be written: with "Resize", the long side is shrunk to
  // the chosen length (never enlarged). Checked here, before any work, so a
  // picture JPEG can't hold gets a message that says what to do about it.
  const Dims outSize =
      fitLongEdge(state.image->width, state.image->height,
                  state.exportResize ? state.exportLongEdge : 0);
  if (jpeg && !liteui_image::jpegSizeOk(static_cast<uint32_t>(outSize.w),
                                        static_cast<uint32_t>(outSize.h))) {
    setStatus("Too large for JPEG (" +
                  std::to_string(liteui_image::kJpegMaxSide) +
                  " px per side at most): turn on Resize, or export as PNG",
              true);
    return;
  }
  const bool resized =
      outSize.w != state.image->width || outSize.h != state.image->height;

  std::string err;
  bool ok = false;
  if (png16) {
    // The picture on screen is 8-bit, so render the current source again with
    // the same edits, rounded to 16 bits once at the end (render_core.hpp), and
    // scale it the same way.
    std::shared_ptr<Image16> big =
        state.source16 ? renderToImage16(*state.source16, state.source.get(),
                                         state.adjust.buildRenderParams())
        : state.source
            ? renderToImage16(*state.source, state.adjust.buildRenderParams())
            : nullptr;
    if (big && resized)
      big = resizeToLongEdge(big, state.exportLongEdge);
    if (big) {
      const std::optional<ImageBuffer> buf = toImageBuffer(*big);
      ok = buf && liteui_image::encodePng(out, *buf, &err, exportPngOptions());
      if (!buf)
        err = "nothing to write";
    } else
      err = "couldn't render the picture";
  } else {
    const std::shared_ptr<CanvasImage> outImage =
        resized ? resizeToLongEdge(state.image, state.exportLongEdge)
                : state.image;
    if (jpeg) {
      ok = saveJpegFile(out, *outImage, state.jpegQuality, state.keepMetadata,
                        state.removeGps, vcImageOf(state.imagePath), &err);
    } else {
      const std::optional<ImageBuffer> buf = toImageBuffer(*outImage);
      ok = buf && liteui_image::encodePng(out, *buf, &err, exportPngOptions());
      if (!buf)
        err = "nothing to write";
    }
  }
  // A PNG export carries its source's color tags (io/color_tag.hpp). A PNG
  // source's profile can't go into a JPEG (it is deflate-compressed), which the
  // status says.
  std::string tagNote;
  if (ok && !jpeg) {
    color_tag::dropEncoderColor(out); // only the source's tags may stay
    const std::string kept =
        color_tag::carryColorToPng(out, vcImageOf(state.imagePath));
    if (!kept.empty())
      tagNote = " [" + kept + " kept]";
  } else if (ok && jpeg && !isJpegPath(vcImageOf(state.imagePath)) &&
             !color_tag::colorTagOf(vcImageOf(state.imagePath)).srgb)
    tagNote = " (color profile not carried into JPEG)";
  if (ok)
    setStatus("Exported " + baseName(out) +
                  std::string(png16 ? " [16-bit]" : "") + tagNote +
                  (resized ? " (" + std::to_string(outSize.w) + " × " +
                                 std::to_string(outSize.h) + ")"
                           : std::string()),
              false);
  else
    setStatus("Export failed: " +
                  (err.empty() ? "couldn't write " + baseName(out) : err),
              true);
}

inline void toggleSidebar() {
  state.activeActivity =
      (state.activeActivity == kNoActivity) ? kInfoActivity : kNoActivity;
  state.markDirty(); // the crop overlay shows only while the Crop pane is open
}