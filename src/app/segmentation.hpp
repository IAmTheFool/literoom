// app/segmentation.hpp: object segmentation worker and engine info
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Object segmentation (background) ----------------
struct SegmentRequest {
  std::shared_ptr<const CanvasImage> image; // the untouched original
  uint64_t imageKey = 0;                    // state.imageSerial
  std::vector<ObjectPrompt> prompts;        // original-picture fractions
  int maskIdx = -1, part = -1;              // where the result goes
  bool prefetch = false; // encode only (warm the model), no result map
};

// ---- Segmentation engine info (SAM model, with the color-grow wand of
// seg_basic.hpp as fallback) ----

// Which engine is active, shown in the Object panel (written by the worker,
// read by the UI thread's label poll).
static std::mutex gSegInfoMutex;
static std::string gSegInfo = "Engine: loads on first use";
static void setSegInfo(std::string s) {
  std::lock_guard<std::mutex> lk(gSegInfoMutex);
  gSegInfo = std::move(s);
}
static std::string getSegInfo() {
  std::lock_guard<std::mutex> lk(gSegInfoMutex);
  return gSegInfo;
}

// Models: $LITEROOM_MODELS, else a "models" folder next to the executable.
static std::string samModelDir() {
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

// Worker thread only. Prefetch (no prompts): load + encode, return null with
// no error. Falls back to the color-grow stand-in when SAM is unavailable.
static std::shared_ptr<const ObjectMap>
segmentObject(const CanvasImage &img, uint64_t key,
              const std::vector<ObjectPrompt> &prompts, std::string *err,
              const std::atomic<uint64_t> *latest, uint64_t gen) {
  static sam::Segmenter seg;
  static int backend = 0; // 0 untried, 1 SAM, -1 fallback
  static uint64_t encodedKey = 0;
  if (backend == 0) {
    const std::string dir = samModelDir();
    std::string e;
    if (seg.load(dir + "/mobile_sam.encoder.onnx",
                 dir + "/mobile_sam.decoder.onnx", &e)) {
      backend = 1;
      setSegInfo("Engine: MobileSAM");
    } else {
      backend = -1;
      setSegInfo("Engine: basic color grow (" + e + "; looked in " + dir + ")");
    }
  }
  if (backend < 0)
    return prompts.empty()
               ? nullptr
               : segmentObjectWand(img, key, prompts, err, latest, gen);

  if (encodedKey != key) {
    if (!seg.encode(img.pixels.data(), img.width, img.height, err))
      return nullptr;
    encodedKey = key;
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
  if (!seg.decode(pts, &logits, &w, &h, err))
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

struct SegmentResult {
  SegmentRequest req;                   // image dropped; prompts + target kept
  std::shared_ptr<const ObjectMap> map; // null on failure
  std::string error;
};

// Worker thread only. Always returns a value (null `map` + `error` on failure).
static std::optional<SegmentResult> runSegmentJob(SegmentRequest &&req,
                                                  WorkerCancel c) {
  SegmentResult r;
  r.map = segmentObject(*req.image, req.imageKey, req.prompts, &r.error,
                        c.latest, c.gen);
  r.req = std::move(req);
  r.req.image.reset();
  return r;
}

// Same protocol as RenderWorker / DecodeWorker (see latest_worker.hpp).
using SegmentWorker = LatestWorker<SegmentRequest, SegmentResult>;

static SegmentWorker segmentWorker{runSegmentJob};

// UI thread: commits a finished segmentation. Prompts and map land in ONE
// undo step. A result for a mask/part that is gone (deleted, undone away) is
// dropped.
static void adoptSegment(SegmentWorker::Result &&r) {

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
static void collectSegmentJob() {
  if (auto r = segmentWorker.collect())
    adoptSegment(std::move(*r));
}

// Swaps a finished decode into the editor (UI thread only).
static void filmMarkEdited(const std::string &path); // filmstrip (below)
static int filmViewPos(int i);                       // filmstrip (below)
static void filmApplyMeta(const std::string &path, const PhotoMeta &m); // ditto

