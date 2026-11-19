// app/geometry_worker.hpp: the background geometry worker and its UI-thread glue
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// BEGIN geometryworker
// ---------------- Background geometry worker ----------------
// Runs the same pipeline syncSource() + renderAdjusted run
// synchronously (orient -> straighten -> crop -> render) off the UI thread
// for a large photo, so clicking Rotate or Straighten's nudge buttons on a
// 24-megapixel image doesn't freeze the window the way releasing a slider
// used to before RenderWorker.
//
// Same thread model and cancellation contract as RenderWorker (one lazily
// started thread; a generation number supersedes stale work; the running
// job polls it and abandons within a few scanlines of being superseded).
// The one real difference: RenderWorker only re-renders pixels for a source
// that hasn't changed, so it can start from `state.source`. A geometry job
// might be changing the source itself, so it always rebuilds the WHOLE
// pipeline from `original` — even a job that only changed the crop redoes
// an unchanged straighten. That wastes some work when angle is non-zero and
// only the crop is edited afterwards, but keeps a job a pure function of
// its inputs, with nothing that can drift out of sync
// work only costs latency on a background thread, never a UI freeze.
// The thread and the generation protocol live in latest_worker.hpp; this is
// the job. `original` must never be modified afterwards (the editor's images
// are replaced, never edited in place - the same rule RenderJob::src relies
// on).

// Exactly the geometry-derived EditorState fields, adopted as one atomic swap
// on the UI thread (see adoptGeometryBundle) so the picture, its document
// size, and the crop selection always change together.
struct GeometryBundle {
  std::shared_ptr<CanvasImage> oriented;
  Orient orientedFor;
  std::shared_ptr<CanvasImage> orientedPreview;
  std::shared_ptr<CanvasImage> rotated;
  float rotatedAngle = 0.0f;
  bool rotatedExpand = false;
  std::shared_ptr<CanvasImage> source;
  CropRect sourceCrop;
  std::shared_ptr<CanvasImage> previewBase;
  std::shared_ptr<Image16>
      previewBase16; // the 16-bit twin of previewBase (null for an 8-bit file
                     // or a small picture)
  std::shared_ptr<CanvasImage> image;
  std::shared_ptr<Image16> oriented16, rotated16,
      source16; // the 16-bit twin (null for an 8-bit file)
};
struct GeometryResult {
  GeometryBundle bundle;
};
struct GeometryJob {
  // Non-const, unlike RenderJob::src: orientImage (identity case) and the rest
  // of this pipeline are written in terms of the same shared_ptr<CanvasImage>
  // type `state.original` already is, so this just carries that type through -
  // never a license to mutate it.
  std::shared_ptr<CanvasImage> original;
  Orient orient;
  float angle = 0.0f;
  bool expand = false;
  CropRect crop;
  RenderParams renderParams;
  std::shared_ptr<Image16>
      original16; // the 16-bit twin of `original`, if the picture has one
};

// The pipeline itself (worker thread only). Checked for cancellation between
// every stage, and by row/block inside the two per-pixel passes
// (rotateArbitrary, renderWith) - so a job superseded mid-straighten or
// mid-render abandons within a few milliseconds rather than finishing
// unwanted work.
static std::optional<GeometryResult> runGeometryJob(GeometryJob &&job,
                                                    WorkerCancel c) {
  if (c.cancelled())
    return std::nullopt;
  GeometryBundle b;
  b.oriented = orientImage(job.original, job.orient); // identity: no copy
  b.orientedFor = job.orient;
  if (c.cancelled())
    return std::nullopt;
  b.orientedPreview = makePreviewBase(*b.oriented);
  if (c.cancelled())
    return std::nullopt;
  if (job.angle == 0.0f) {
    b.rotated = b.oriented;
  } else {
    b.rotated =
        rotateArbitrary(*b.oriented, job.angle, job.expand, c.latest, c.gen);
    if (!b.rotated)
      return std::nullopt; // abandoned mid-straighten
  }
  b.rotatedAngle = job.angle;
  b.rotatedExpand = job.expand;
  if (c.cancelled())
    return std::nullopt;
  b.sourceCrop = job.crop;
  b.source = job.crop.empty() ? b.rotated : cropImage(*b.rotated, job.crop);
  if (c.cancelled())
    return std::nullopt;
  if (job.original16) { // the 16-bit twin of the same chain; the render below
                        // starts from it
    b.oriented16 = wide_chain::oriented(job.original16, job.orient);
    if (c.cancelled())
      return std::nullopt;
    b.rotated16 = wide_chain::rotated(b.oriented16, job.angle, job.expand,
                                      c.latest, c.gen);
    if (!b.rotated16)
      return std::nullopt; // abandoned mid-straighten
    b.source16 = wide_chain::cropped(b.rotated16, job.crop);
    if (c.cancelled())
      return std::nullopt;
  }
  b.previewBase = makePreviewBase(*b.source);
  b.previewBase16 = b.source16 ? makePreviewBase(*b.source16) : nullptr;
  b.image = b.source16
                ? renderToDisplay(*b.source16, b.source.get(), job.renderParams,
                                  c.latest, c.gen)
                : renderWith(*b.source, job.renderParams, c.latest, c.gen);
  if (!b.image)
    return std::nullopt; // abandoned mid-render
  return GeometryResult{std::move(b)};
}

using GeometryWorker = LatestWorker<GeometryJob, GeometryResult>;
// END geometryworker

static GeometryWorker geometryWorker{runGeometryJob};

// True when state.adjust's committed orientation/angle/expand/crop hasn't
// been rendered into state.oriented/rotated/source yet — mirrors syncSource's
// own dirty checks, but as a read-only predicate (no rebuilding). Folds in
// "a geometry job is already running" too: that can only be true right after
// this function itself returned true and a submit followed, so the two are
// never actually independent — but checking both directly, rather than
// relying on that invariant, means a bug elsewhere fails safe (one wasted
// resubmit) instead of silently letting a stale in-flight job get adopted.
static bool geometryDirty() {
  if (geometryWorker.outstanding())
    return true;
  if (!state.oriented || !state.rotated)
    return true;
  const Orient orient = state.adjust.orient();
  const float angle = state.adjust.angle();
  const bool expand = state.adjust.expand();
  if (orient != state.orientedFor)
    return true;
  if (angle != state.rotatedAngle)
    return true;
  if (angle != 0.0f && expand != state.rotatedExpand)
    return true;
  return state.adjust.crop() != state.sourceCrop;
}

static void submitGeometryJob() {
  geometryWorker.submit(
      GeometryJob{state.original, state.adjust.orient(), state.adjust.angle(),
                  state.adjust.expand(), state.adjust.crop(),
                  state.adjust.buildRenderParams(), state.original16});
  setStatus("Applying changes...", /*isError=*/false);
}

// Swaps in every field of a finished geometry rebuild together, so the
// picture, its document size, and the crop selection never show a
// combination that belonged to two different requests.
static void adoptGeometryBundle(GeometryWorker::Result &&r) {
  GeometryBundle &b = r.bundle;
  state.oriented = std::move(b.oriented);
  state.orientedFor = b.orientedFor;
  state.orientedPreview = std::move(b.orientedPreview);
  state.rotated = std::move(b.rotated);
  state.rotatedAngle = b.rotatedAngle;
  state.rotatedExpand = b.rotatedExpand;
  state.source = std::move(b.source);
  state.sourceCrop = b.sourceCrop;
  state.previewBase = std::move(b.previewBase);
  state.previewBase16 = std::move(b.previewBase16);
  state.oriented16 = std::move(b.oriented16);
  state.rotated16 = std::move(b.rotated16);
  state.source16 = std::move(b.source16);
  state.image = std::move(b.image);
  state.previewImage.reset(); // the real thing replaces any stand-in
  state.previewDocW = state.previewDocH = 0.0f;
  resetCropSelection(); // the crop UI must match the new source's dimensions
  state.histStale = true;
  clearStatus(); // the "Applying changes..." message is no longer true
  if (state.colorNoticePending) {
    state.colorNoticePending = false;
    announceColorTag(); // a freshly opened picture's color notice, wiped by the
                        // line above
  }
  state.markDirty();
}

// Non-blocking; polled once per frame from flushPendingAdjust, alongside
// collectFullRender.
static void collectGeometryJob() {
  if (auto r = geometryWorker.collect())
    adoptGeometryBundle(std::move(*r));
}

// Blocking: makes every field a geometry rebuild touches current. For the
// one place that needs the real pixels *now* (export).
static void finishGeometryJob() {
  if (!geometryWorker.outstanding())
    return;
  adoptGeometryBundle(geometryWorker.waitForLatest());
}

// True while a crop/rotate/flip/straighten rebuild is still running. Until
// it lands, state.rotated / state.source / the crop selection describe the
// PREVIOUS geometry while state.adjust already holds the new one, so any
// edit that measures against them (a second rotate remapping the crop rect,
// Apply Crop reading the selection) would compute from stale sizes. Those
// edits are ignored while this is true; their buttons are dimmed to match.
static bool geometryBusy() { return geometryWorker.outstanding(); }

