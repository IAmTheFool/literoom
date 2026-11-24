// app/geometry_worker.hpp: the UI-thread glue of the geometry worker
//
// The job itself (GeometryJob, GeometryBundle, runGeometryJob) is pure and lives
// in workers/geometry_job.hpp; the worker object is workers().geometry
// (app/workers.hpp). What is left here reads and writes `state`: deciding
// whether a rebuild is needed, submitting it, and adopting the finished bundle
// as one atomic swap.
//
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

#include "workers/geometry_job.hpp"

// True when state.adjust's committed orientation/angle/expand/crop hasn't
// been rendered into state.oriented/rotated/source yet — mirrors syncSource's
// own dirty checks, but as a read-only predicate (no rebuilding). Folds in
// "a geometry job is already running" too: that can only be true right after
// this function itself returned true and a submit followed, so the two are
// never actually independent — but checking both directly, rather than
// relying on that invariant, means a bug elsewhere fails safe (one wasted
// resubmit) instead of silently letting a stale in-flight job get adopted.
static bool geometryDirty() {
  if (workers().geometry.outstanding())
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
  workers().geometry.submit(
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
  try {
    if (auto r = workers().geometry.collect())
      adoptGeometryBundle(std::move(*r));
  } catch (const std::exception &e) {
    setStatus(std::string("Rebuild failed: ") + e.what(), /*isError=*/true);
  }
}
// Blocking: makes every field a geometry rebuild touches current. For the
// one place that needs the real pixels *now* (export).
static void finishGeometryJob() {
  if (!workers().geometry.outstanding())
    return;

  auto result = workers().geometry.waitForLatest();

  switch (result.status) {
  case GeometryWorker::WaitStatus::completed:
    if (!result.value)
      throw std::logic_error("GeometryWorker completed without a result");

    adoptGeometryBundle(std::move(*result.value));
    return;

  case GeometryWorker::WaitStatus::cancelled:
    // The bundle is obsolete or was abandoned. Do not partially
    // update geometry-related EditorState fields.
    throw std::runtime_error(
        "Geometry rebuild was cancelled before completion");

  case GeometryWorker::WaitStatus::failed:
    if (result.error)
      std::rethrow_exception(result.error);

    throw std::runtime_error("GeometryWorker failed without an exception");
  }

  throw std::logic_error("Unknown GeometryWorker wait status");
}

// True while a crop/rotate/flip/straighten rebuild is still running. Until
// it lands, state.rotated / state.source / the crop selection describe the
// PREVIOUS geometry while state.adjust already holds the new one, so any
// edit that measures against them (a second rotate remapping the crop rect,
// Apply Crop reading the selection) would compute from stale sizes. Those
// edits are ignored while this is true; their buttons are dimmed to match.
static bool geometryBusy() { return workers().geometry.outstanding(); }
