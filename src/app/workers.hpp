// app/workers.hpp - every background worker of the editor, owned in one place.
//
// Before this, each worker was a `static` object defined in its own app header.
// A `static` in a header is one object PER translation unit: the day a second
// .cpp included one of them it would have got its own copy, with its own thread,
// its own queue and its own "busy" answer. It only worked because main.cpp
// includes everything (one TU). Now there is exactly one Workers, created by
// workers() below, and the job types and job functions (the part that is pure
// and testable) live in workers/*.hpp.
//
// Replaces: app/render_worker.hpp, app/decode.hpp, and the worker objects in
// app/geometry_worker.hpp, app/segmentation.hpp, app/sync.hpp, app/export.hpp,
// app/lut_pane.hpp and app/filmstrip.hpp (their UI-thread glue stays there).
//
// Included by main.cpp where app/render_worker.hpp used to be, i.e. before
// EditorState, because EditorState::showPreview() asks the render worker.
#pragma once
#include <functional>
#include <string>

#include "io/image_meta.hpp" // exifOrientationOf
#include "workers/decode_job.hpp"
#include "workers/export_worker.hpp"
#include "workers/geometry_job.hpp"
#include "workers/lut_preview_worker.hpp"
#include "workers/render_job.hpp"
#include "workers/segment_job.hpp"
#include "workers/sync_job.hpp"
#include "workers/thumb_worker.hpp"

// Thread model: every worker is driven from the UI thread only (see each
// worker's header); the notify callback is the one thing that crosses threads.
//
// Member order matters. Members are destroyed in reverse order, and a worker's
// destructor joins its thread, so `sam` (declared first) outlives the segment
// worker that borrows it. Anything the notify callback captures (the LiteUI
// window) must outlive these threads: call shutdown() before it goes away.
struct Workers {
  Workers() = default;
  Workers(const Workers &) = delete; // threads, mutexes and a `this` capture
  Workers &operator=(const Workers &) = delete;
  ~Workers() { shutdown(); }

  SamEngine sam; // borrowed by `segment`: keep it declared BEFORE it

  RenderWorker render{runRenderJob};
  GeometryWorker geometry{runGeometryJob};
  DecodeWorker decode;
  SegmentWorker segment{[this](SegmentRequest &&req, WorkerCancel c) {
    return runSegmentJob(sam, std::move(req), c);
  }};
  SyncWorker sync{runSyncJob};
  ExportWorker exporter;
  // Thumbnails are turned upright with the editor's own EXIF reader.
  ThumbWorker thumb{[](const std::string &path) {
    return exifOrientationOf(path);
  }};
  LutPreviewWorker lutPreview;

  // One callback for all eight: a finished job wakes the (otherwise sleeping)
  // event loop. Safe to call at any time (each worker copies it under its own
  // lock); normally once at startup.
  void setNotify(const std::function<void()> &wake) {
    render.setNotify(wake);
    geometry.setNotify(wake);
    decode.setNotify(wake);
    segment.setNotify(wake);
    sync.setNotify(wake);
    exporter.setNotify(wake);
    thumb.setNotify(wake);
    lutPreview.setNotify(wake);
  }

  // Stops and joins every worker thread. After this no notify callback runs
  // again. Idempotent; the destructor calls it too.
  void shutdown() {
    render.shutdown();     // abandons the render in flight
    geometry.shutdown();   // ditto
    decode.shutdown();     // waits out a decode still running (can't be cancelled)
    thumb.shutdown();
    segment.shutdown();
    sync.shutdown();       // finishes the batch it is writing
    exporter.shutdown();   // abandons the picture it is rendering
    lutPreview.shutdown();
  }
};

// The one Workers of the program (created on first use, thread-safe). `state` is
// still a global too; if the editor ever needs several of either (tests, several
// windows), both would move into an App that main() owns and passes down.
inline Workers &workers() {
  static Workers instance;
  return instance;
}

// Declare one of these in main() right after the window and the notify wiring.
// Its destructor joins every worker thread when main() is left (normally, or by
// an exception), before the window that the notify callback captured is
// destroyed: members are destroyed in reverse order of declaration, so a guard
// declared after the window always goes first. No worker thread can then call
// ui.wake() on a dead window, and nothing has to remember a shutdown call.
struct WorkersShutdownGuard {
  WorkersShutdownGuard() = default;
  WorkersShutdownGuard(const WorkersShutdownGuard &) = delete;
  WorkersShutdownGuard &operator=(const WorkersShutdownGuard &) = delete;
  ~WorkersShutdownGuard() { workers().shutdown(); }
};

// The segmentation engine's label for the Object panel (any thread).
inline std::string getSegInfo() { return workers().sam.info(); }