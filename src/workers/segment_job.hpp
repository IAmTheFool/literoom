// workers/segment_job.hpp - object segmentation: the SAM engine and the job.
//
// Moved out of app/segmentation.hpp. The old segmentObject() kept its state in
// function-local statics (the loaded model, which backend is in use, which
// picture the encoder holds) plus a global label guarded by a mutex. All of
// that is now one SamEngine object, owned by `Workers` (app/workers.hpp)
// and borrowed by the segment worker's job function, so there is exactly one,
// it has an owner, and it is destroyed after the worker thread that uses it
// has been joined. The UI-thread glue (adoptSegment, collectSegmentJob) stays
// in app/segmentation.hpp.
//
// Engines: MobileSAM, with the color-grow wand of mask/seg_basic.hpp as the
// fallback when the model can't be loaded.
//
// Failure handling.
//  * Loading the model: a load that THROWS (the ONNX runtime reports a bad or
//    missing model that way) is the same as one that returns false: SAM is
//    unavailable and the color-grow wand takes over, with the reason in the
//    engine label. If it were left to propagate, the backend would stay
//    "untried" and every later job would try the load (and fail) all over again.
//  * Encoding: the encoder's cache key is cleared BEFORE the call and set only
//    after it succeeds. A throw or a failure part-way leaves the encoder in an
//    unknown state, and a stale key would let the next job for the previous
//    picture skip the encode and decode against it.
//  * Anything else thrown (decoding, the wand, an allocation) is caught by
//    runSegmentJob and reported through SegmentResult::error.
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX // windows.h must not define min()/max() macros (std::min/max)
#endif
#include <windows.h> // MAX_PATH, GetModuleFileNameW, WideCharToMultiByte
#else
#include <unistd.h> // readlink
#endif

#include "liteui.hpp"
#include "mask/mask.hpp"
#include "mask/sam_segmenter.hpp"
#include "mask/seg_basic.hpp"
#include "workers/latest_worker.hpp"

// logit -> coverage steepness (edge softness)
inline constexpr float kSamSoftness = 2.0f;

struct SegmentRequest {
  std::shared_ptr<const CanvasImage> image; // the untouched original
  uint64_t imageKey = 0;                    // state.imageSerial
  std::vector<ObjectPrompt> prompts;        // original-picture fractions
  int maskIdx = -1, part = -1;              // where the result goes
  bool prefetch = false; // encode only (warm the model), no result map
};

struct SegmentResult {
  SegmentRequest req;                   // image dropped; prompts + target kept
  std::shared_ptr<const ObjectMap> map; // null on failure
  std::string error;
};

// Models: $LITEROOM_MODELS, else a "models" folder next to the executable.
inline std::string samModelDir() {
  if (const char *e = std::getenv("LITEROOM_MODELS"); e && *e)
    return e;
#if defined(_WIN32)
  wchar_t wbuf[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, wbuf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    std::wstring w(wbuf, n);
    const size_t slash = w.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
      w.resize(slash);
    const int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                        nullptr, nullptr);
    std::string s(len > 0 ? static_cast<size_t>(len - 1) : 0, '\0');
    if (len > 0)
      WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr,
                          nullptr);
    return s + "\\models";
  }
#else
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n > 0) {
    std::string p(buf, static_cast<size_t>(n));
    const size_t slash = p.find_last_of('/');
    if (slash != std::string::npos)
      return p.substr(0, slash) + "/models";
  }
#endif
  return "models";
}

// The segmentation engine. segment() is for the worker thread only (the model
// and its encoder cache are not shared); info() may be called from any thread.
class SamEngine {
public:
  SamEngine() = default;
  SamEngine(const SamEngine &) = delete;
  SamEngine &operator=(const SamEngine &) = delete;

  // Which engine is active, for the Object panel's label (UI thread polls it).
  std::string info() const {
    std::lock_guard<std::mutex> lk(m_);
    return info_;
  }

  // Worker thread only. Prefetch (no prompts): load + encode, return null with
  // no error. Falls back to the color-grow stand-in when SAM is unavailable.
  std::shared_ptr<const ObjectMap>
  segment(const CanvasImage &img, uint64_t key,
          const std::vector<ObjectPrompt> &prompts, std::string *err,
          const std::atomic<uint64_t> *latest, uint64_t gen) {
    if (backend_ == 0)
      loadBackend();
    if (backend_ < 0)
      return prompts.empty()
                 ? nullptr
                 : segmentObjectWand(img, key, prompts, err, latest, gen);

    if (encodedKey_ != key) {
      encodedKey_ = kNoKey; // the encoder is in flux until encode() succeeds
      if (!seg_.encode(img.pixels.data(), img.width, img.height, err))
        return nullptr;
      encodedKey_ = key;
    }
    if (prompts.empty() ||
        (latest && latest->load(std::memory_order_relaxed) != gen))
      return nullptr; // prefetch, or superseded while encoding

    std::vector<sam::Point> pts;
    pts.reserve(prompts.size());
    for (const ObjectPrompt &p : prompts)
      pts.push_back({p.x, p.y, p.fg});
    std::vector<float> logits;
    int w = 0, h = 0;
    if (!seg_.decode(pts, &logits, &w, &h, err))
      return nullptr;
    auto m = std::make_shared<ObjectMap>();
    m->w = w;
    m->h = h;
    m->id = nextObjectMapId();
    m->a.resize(logits.size());
    for (size_t i = 0; i < logits.size(); ++i) // soft edge: sigmoid of the logit
      m->a[i] = static_cast<uint8_t>(
          std::lround(255.0f / (1.0f + std::exp(-logits[i] * kSamSoftness))));
    return m;
  }

private:
  static constexpr uint64_t kNoKey = ~uint64_t{0};

  void setInfo(std::string s) {
    std::lock_guard<std::mutex> lk(m_);
    info_ = std::move(s);
  }

  // First use: try MobileSAM; any failure (false OR a throw) means the wand.
  void loadBackend() {
    const std::string dir = samModelDir();
    std::string e;
    bool loaded = false;
    try {
      loaded = seg_.load(dir + "/mobile_sam.encoder.onnx",
                         dir + "/mobile_sam.decoder.onnx", &e);
    } catch (const std::exception &ex) {
      loaded = false;
      e = ex.what();
    } catch (...) {
      loaded = false;
      e = "unexpected error while loading the model";
    }
    if (loaded) {
      backend_ = 1;
      setInfo("Engine: MobileSAM");
    } else {
      backend_ = -1;
      setInfo("Engine: basic color grow (" + e + "; looked in " + dir + ")");
    }
  }

  sam::Segmenter seg_;
  int backend_ = 0;                // 0 untried, 1 SAM, -1 fallback (worker only)
  uint64_t encodedKey_ = kNoKey;   // the picture the encoder holds (worker only)
  mutable std::mutex m_;           // guards info_
  std::string info_ = "Engine: loads on first use";
};

// Worker thread only. Always returns a value (null `map` + `error` on failure).
// An exception from the segmenter is caught here and becomes `error`, which
// adoptSegment already shows ("Object selection failed: ..." / "... unavailable:
// ..." for a prefetch): LatestWorker would otherwise park it and rethrow it
// from collect() on the UI thread, where nothing catches it.
inline std::optional<SegmentResult>
runSegmentJob(SamEngine &engine, SegmentRequest &&req, WorkerCancel c) {
  SegmentResult r;
  try {
    if (!req.image)
      r.error = "no picture to analyze";
    else
      r.map = engine.segment(*req.image, req.imageKey, req.prompts, &r.error,
                             c.latest, c.gen);
  } catch (const std::bad_alloc &) {
    r.map = nullptr;
    r.error = "not enough memory";
  } catch (const std::exception &e) {
    r.map = nullptr;
    r.error = e.what();
    if (r.error.empty())
      r.error = "unexpected error";
  } catch (...) {
    r.map = nullptr;
    r.error = "unexpected error";
  }
  r.req = std::move(req);
  r.req.image.reset();
  return r;
}

// Same protocol as the other workers (see latest_worker.hpp).
using SegmentWorker = LatestWorker<SegmentRequest, SegmentResult>;
