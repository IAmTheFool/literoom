// workers/decode_job.hpp - opening a file: the background decode job.
//
// Moved out of app/decode.hpp (which is now gone). Decoding a big PNG/JPEG takes
// long enough to freeze the window, so it runs on its own thread. Same contract
// as the render and geometry workers: submit() supersedes any earlier request,
// the finished result is parked until the UI thread collect()s it, and the
// notify callback wakes the sleeping event loop. Until the result lands the
// previously open image stays fully usable; nothing about the editor state
// changes until adoptDecoded() (app/filmstrip.hpp) swaps the new file in. The
// one DecodeWorker lives in `Workers` (app/workers.hpp).
//
// A decode already running can't be interrupted (the image libraries have no
// cancel hook), so opening a second file queues behind it and the first
// file's result is simply dropped when it finishes.
//
// Failure handling: a decode that throws (std::bad_alloc on a huge picture, a
// library exception on a corrupt one) must not become an exception on the UI
// thread: LatestWorker would park it and rethrow it from collect(), where the
// open-file code expects an ordinary "couldn't open it" result instead. So the
// job catches it and reports it the way every other failure is reported: an
// empty `image` plus `error`. The previously open picture stays open.
#pragma once
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <string>

#include "core/image16.hpp"
#include "io/image_meta.hpp"   // exifOrientationOf
#include "io/image_source.hpp" // openUpright
#include "io/virtual_copy.hpp" // vcImageOf
#include "liteui.hpp"
#include "workers/latest_worker.hpp"

struct DecodeJob {
  std::string path;
};
struct DecodeResult {
  std::string path;
  std::optional<CanvasImage>
      image;         // empty on failure (for a 16-bit file: its 8-bit form)
  std::string error; // library message when image is empty
  std::shared_ptr<Image16> wide; // a 16-bit PNG at full precision (upright);
                                 // null for every other file
};

// Worker thread only. Always returns a value (a failure is an empty `image`
// plus `error`): a decode can't be cancelled part-way.
inline std::optional<DecodeResult> runDecodeJob(DecodeJob &&job, WorkerCancel) {
  DecodeResult r;
  r.path = job.path;
  try {
    const std::string real = vcImageOf(job.path); // a copy decodes its picture
    // A camera's rotation (EXIF) is applied here, once: from now on the
    // picture is upright everywhere (editor, masks, saved edits, export). A
    // 16-bit file is read at full precision into `wide`, and its 8-bit form
    // (what the tools measure) is made from it. See io/image_source.hpp.
    if (auto opened = image_source::openUpright(real, exifOrientationOf(real),
                                                /*want8=*/true, &r.error)) {
      if (opened->image) {
        r.image.emplace(std::move(*opened->image));
        r.wide = std::move(opened->wide);
      } else if (r.error.empty())
        r.error = "couldn't open it"; // opened, but with no picture in it
    }
  } catch (const std::bad_alloc &) {
    r.image.reset();
    r.wide.reset();
    r.error = "not enough memory to open this picture";
  } catch (const std::exception &e) {
    r.image.reset();
    r.wide.reset();
    r.error = e.what();
    if (r.error.empty())
      r.error = "unexpected error";
  } catch (...) {
    r.image.reset();
    r.wide.reset();
    r.error = "unexpected error";
  }
  return r;
}

// LatestWorker plus the UI-thread bookkeeping of which file is being decoded.
class DecodeWorker : public LatestWorker<DecodeJob, DecodeResult> {
public:
  DecodeWorker() : LatestWorker<DecodeJob, DecodeResult>(runDecodeJob) {}

  void submit(std::string path) {
    pendingPath_ = path;
    LatestWorker<DecodeJob, DecodeResult>::submit(DecodeJob{std::move(path)});
  }

  // The file the newest request is decoding (UI thread; valid while
  // outstanding()).
  const std::string &pendingPath() const { return pendingPath_; }

private:
  std::string pendingPath_; // UI thread only
};
