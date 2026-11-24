// workers/render_job.hpp - the render worker's job: the full-resolution pixel
// pass, run off the UI thread so releasing a slider on a 24-megapixel photo
// doesn't freeze the window.
//
// Moved out of app/render_worker.hpp. This part is pure: no `state`, no LiteUI
// calls. Its only inputs are an immutable source image and a RenderParams copy;
// its only output is the result LatestWorker parks. Because nothing here
// defines an object, any file may include it; the one RenderWorker lives in
// `Workers` (app/workers.hpp). The UI-thread glue (adoptFullRender,
// collectFullRender, ...) stays in app/render_update.hpp.
//
// Stale work is handled by LatestWorker's generation number: the running render
// polls it (see renderWith) and abandons itself within a few milliseconds of
// being superseded, so jobs are never queued up.
#pragma once
#include <memory>
#include <optional>

#include "core/image16.hpp"
#include "edit/render_params.hpp"
#include "liteui.hpp"
#include "render/render.hpp"
#include "workers/latest_worker.hpp"

struct RenderJob {
  std::shared_ptr<const CanvasImage> src;
  RenderParams params;
  // The 16-bit twin of `src` (EditorState::source16), if the picture has one:
  // the render starts from it and reads it for the neighbor effects too; `src`
  // is then only its 8-bit view (what the tools measure).
  std::shared_ptr<const Image16> src16;
};
struct RenderResult {
  std::shared_ptr<CanvasImage> image;
};

// Worker thread only. A render that throws is stored by LatestWorker and
// rethrown from collect(); collectFullRender catches it there.
inline std::optional<RenderResult> runRenderJob(RenderJob &&job,
                                                WorkerCancel c) {
  auto img = job.src16 ? renderToDisplay(*job.src16, job.src.get(), job.params,
                                         c.latest, c.gen)
                       : renderWith(*job.src, job.params, c.latest, c.gen);
  if (!img)
    return std::nullopt; // abandoned part-way: superseded
  return RenderResult{std::move(img)};
}

// `src` in a submitted RenderJob must never be modified afterwards (the
// editor's source images are replaced, never edited in place).
using RenderWorker = LatestWorker<RenderJob, RenderResult>;
