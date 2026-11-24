// workers/geometry_job.hpp - the geometry worker's job: the same pipeline
// syncSource() + renderAdjusted run synchronously (orient -> straighten ->
// crop -> render), off the UI thread, so clicking Rotate or Straighten's nudge
// buttons on a 24-megapixel image doesn't freeze the window.
//
// Moved out of app/geometry_worker.hpp. This part is pure (no `state`); the
// UI-thread glue (geometryDirty, submitGeometryJob, adoptGeometryBundle, ...)
// stays in app/geometry_worker.hpp. The one GeometryWorker lives in `Workers`
// (app/workers.hpp).
//
// Same thread model and cancellation contract as the render worker (a
// generation number supersedes stale work; the running job polls it and
// abandons within a few scanlines of being superseded). The one real
// difference: the render worker only re-renders pixels for a source that
// hasn't changed, so it can start from `state.source`. A geometry job might be
// changing the source itself, so it always rebuilds the WHOLE pipeline from
// `original` - even a job that only changed the crop redoes an unchanged
// straighten. That wastes some work when the angle is non-zero and only the
// crop is edited afterwards, but keeps a job a pure function of its inputs,
// with nothing that can drift out of sync; the cost is latency on a background
// thread, never a UI freeze. `original` must never be modified afterwards (the
// editor's images are replaced, never edited in place).
#pragma once
#include <memory>
#include <optional>

#include "core/geometry.hpp"
#include "core/image16.hpp"
#include "core/raster_ops.hpp"
#include "core/wide_chain.hpp"
#include "edit/render_params.hpp"
#include "liteui.hpp"
#include "render/render.hpp"
#include "workers/latest_worker.hpp"

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
//
// makePreviewBase (used below) must be declared by one of the headers above;
// if the compiler says it isn't, include the header that defines it here.
inline std::optional<GeometryResult> runGeometryJob(GeometryJob &&job,
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
