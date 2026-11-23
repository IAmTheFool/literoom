// app/render_worker.hpp: the background render worker (fence: renderworker)
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once
#include "workers/latest_worker.hpp"
// BEGIN renderworker
// ---------------- Background render worker ----------------
// Runs the full-resolution pixel pass off the UI thread so releasing a slider
// on a 24-megapixel photo doesn't freeze the window.
//
// Thread model: everything public here is called from the UI thread only. The
// one worker thread (started lazily on the first submit, so small images
// never spawn it) sleeps until a job is waiting, renders it, and parks the
// result in a slot. It touches nothing else in the program: no `state`, no
// LiteUI calls. Its only inputs are an immutable source image and a
// RenderParams copy; its only output is the parked result.
//
// Stale work is handled with a generation number. Every submit() and
// invalidate() bumps it; a result is only ever accepted if its generation
// still equals the newest one, and the running render polls the same number
// (see renderWith) so it abandons itself within a few milliseconds of being
// superseded. Only the newest request can win, so jobs are never queued up:
// a newer submit simply replaces one still waiting.
// The thread, the generation protocol and the parking slot live in
// latest_worker.hpp (LatestWorker, tested on its own); what is specific to
// rendering is just the job below.
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

// Worker thread only.
static std::optional<RenderResult> runRenderJob(RenderJob &&job,
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
// END renderworker

static RenderWorker renderWorker{runRenderJob};

