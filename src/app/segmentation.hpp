// app/segmentation.hpp: the UI-thread glue of object segmentation
//
// The engine and the job (SamEngine, SegmentRequest, SegmentResult,
// runSegmentJob) are pure and live in workers/segment_job.hpp; the worker object
// is workers().segment and the engine is workers().sam (app/workers.hpp). What
// is left here commits a finished segmentation into `state`.
//
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// UI thread: commits a finished segmentation. Prompts and map land in ONE
// undo step. A result for a mask/part that is gone (deleted, undone away) is
// dropped.
inline void adoptSegment(SegmentWorker::Result &&r) {

  if (r.req.prefetch) {
    if (!r.error.empty())
      setStatus("Object selection unavailable: " + r.error, true);
    else {
      state.mask.encodedKey = r.req.imageKey;
      clearStatus();
    }
    state.markDirty();
    return;
  }
  state.mask.objPending = {};
  state.markDirty();
  if (!r.map) {
    setStatus("Object selection failed" +
                  (r.error.empty() ? std::string() : ": " + r.error),
              true);
    return;
  }
  MaskSet ms = state.adjust.masks();
  if (r.req.maskIdx < 0 || r.req.maskIdx >= static_cast<int>(ms.size()))
    return;
  Mask &m = ms[static_cast<size_t>(r.req.maskIdx)];
  if (r.req.part < 0 || r.req.part >= static_cast<int>(m.parts.size()))
    return;
  auto *o =
      std::get_if<ObjectShape>(&m.parts[static_cast<size_t>(r.req.part)].shape);
  if (!o)
    return;
  o->map = r.map;
  o->prompts = std::move(r.req.prompts);
  state.mask.encodedKey = r.req.imageKey;
  state.adjust.setMasks(ms, -1);
  clearStatus();
  state.markAdjustDirty(); // re-render through the normal (drag-safe) path
}

// Polled once per frame from flushPendingAdjust.
inline void collectSegmentJob() {
  if (auto r = workers().segment.collect())
    adoptSegment(std::move(*r));
}

// Swaps a finished decode into the editor (UI thread only).
inline void filmMarkEdited(const std::string &path); // filmstrip (below)
inline int filmViewPos(int i);                       // filmstrip (below)
inline void filmApplyMeta(const std::string &path, const PhotoMeta &m); // ditto