// app/render_update.hpp: handing the render worker's result back, drag preview, image refresh
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---- Handing the worker's result back to the UI thread ----
// The event loop sleeps until the OS hands it an event, and a worker finishing
// is not one — so the worker announces itself: RenderWorker::notify_ (wired
// in main() to LiteUI::wake, which is safe from any thread) wakes the loop,
// which polls the canvases, and collectFullRender() below picks the result up
// from there. Nothing runs while no render is outstanding, so an idle editor
// costs nothing.

static void adoptFullRender(RenderWorker::Result &&r) {
  state.image = std::move(r.image);
  state.previewImage.reset(); // the real thing replaces the stand-in
  state.histStale = true;
  state.markDirty();
}

// Non-blocking; polled once per frame from flushPendingAdjust.
static void collectFullRender() {
  if (auto r = renderWorker.collect())
    adoptFullRender(std::move(*r));
}

// Blocking: makes `state.image` current. For the few places that need the
// real pixels *now* (export).
static void finishFullRender() {
  if (!renderWorker.outstanding())
    return;
  adoptFullRender(renderWorker.waitForLatest());
}

// A drag starts (or continues) on the preview path. A full-resolution render
// still in flight would be for values the drag is about to change, so drop it.
static void enterDrag(bool isAngle) {
  if (!state.dragging)
    renderWorker.invalidate();
  state.dragging = true; // switches recompute+paint to the preview path
  state.dragIsAngle = isAngle;
}

// Rebuilds `state.image` (what's drawn and exported) from the untouched
// `state.source` pixels (the original, cropped if a crop is committed)
// using the current adjustment values. Called after every change to
// `state.adjust` — a slider drag, undo, redo, a crop, or opening a new
// image.
//
// Two paths:
//  * Deferred (large images, adjustment-only changes): render the cheap
//    preview now so the picture updates immediately, and let the worker
//    produce the full-resolution `image` in the background. paintCanvas keeps
//    showing the preview (EditorState::showPreview) until it arrives.
//  * Synchronous (everything else): small images, where the pass is faster
//    than the hand-off, and any change that alters the picture's geometry
//    (crop, rotate, straighten, a newly opened file), because then
//    `source`, `image`, the document size and the crop selection must all
//    change together.
static void updateImageFromAdjustments() {
  if (!state.original)
    return;
  clearStatus(); // the image is about to change: an old message is stale
  state.colorNoticePending = false;
  renderWorker.invalidate(); // whatever was in flight is now out of date

  // Orient/straighten/crop are comparatively expensive at full resolution —
  // the bilinear straighten especially — so on a large photo they run on
  // geometryWorker instead of blocking here. geometryDirty() folds in
  // "a job is already running" (see its definition): either way, submit()
  // replaces whatever's in flight, so this is safe to call every time.
  if (isLargeImage(*state.original) && geometryDirty()) {
    submitGeometryJob();
    return; // keep showing the current picture until the bundle lands
  }

  const std::shared_ptr<CanvasImage> sourceBefore = state.source;
  syncSource(false); // picks up a crop change from apply/undo/redo
  const bool sourceChanged = state.source != sourceBefore;
  const bool canDefer = state.previewBase && !sourceChanged && state.image &&
                        state.image->width == state.source->width &&
                        state.image->height == state.source->height;
  if (canDefer) {
    updatePreviewFromAdjustments(); // cheap stand-in, shown immediately
    renderWorker.submit(RenderJob{
        state.source, state.adjust.buildRenderParams(), state.source16});
    return;
  }
  state.image = state.source16
                    ? renderToDisplay(*state.source16, state.source.get(),
                                      state.adjust.buildRenderParams())
                    : renderAdjusted(state.adjust, *state.source);
  state.previewImage.reset(); // superseded by the full-resolution result
  state.previewDocW = state.previewDocH = 0.0f;
  state.histStale = true;
  state.markDirty();
}

// Like updateImageFromAdjustments(), but applied to the small preview
// instead of the full-resolution original. Only touches state.previewImage;
// state.image (and so the document size, export, and info labels) is left
// alone until the drag ends.
static void updatePreviewFromAdjustments() {
  if (!state.previewBase)
    return;
  state.previewImage =
      state.previewBase16
          ? renderToDisplay(*state.previewBase16, state.previewBase.get(),
                            state.adjust.buildRenderParams())
          : renderAdjusted(state.adjust, *state.previewBase);
  state.histStale = true;
  state.markDirty();
}

