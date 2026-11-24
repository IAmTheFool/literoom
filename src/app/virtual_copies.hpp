// app/virtual_copies.hpp: virtual copies
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Virtual copies ----------------
// A virtual copy is another set of edits for the same picture file: its own
// sidecar ("photo.jpg.vc2.literoom.json"), its own filmstrip cell right after
// the picture's, its own rating, flag, label and snapshots. See
// virtual_copy.hpp. Opening one of the open picture's copies reuses the pixels
// (see filmOpenIndex), so switching is instant.

// Puts `e` at index `at` of the strip and keeps every stored index right.
inline void filmInsertEntry(size_t at, FilmEntry e) {
  FilmStrip &f = state.film;
  f.files.insert(f.files.begin() + static_cast<std::ptrdiff_t>(at),
                 std::move(e));
  for (int *i : {&f.current, &f.target, &f.anchor})
    if (*i >= static_cast<int>(at))
      ++*i;
  f.byName.clear();
  for (size_t i = 0; i < f.files.size(); ++i)
    f.byName[filmLower(baseName(f.files[i].path))] = i;
  filmRebuildView();
}

// Takes entry `at` out of the strip. The open picture's index is forgotten
// (-1) when it was the one removed; the caller opens something else.
inline void filmRemoveEntry(size_t at) {
  FilmStrip &f = state.film;
  f.files.erase(f.files.begin() + static_cast<std::ptrdiff_t>(at));
  for (int *i : {&f.current, &f.target, &f.anchor}) {
    if (*i == static_cast<int>(at))
      *i = -1;
    else if (*i > static_cast<int>(at))
      --*i;
  }
  f.byName.clear();
  for (size_t i = 0; i < f.files.size(); ++i)
    f.byName[filmLower(baseName(f.files[i].path))] = i;
  filmRebuildView();
}

// A new virtual copy of the open picture, starting from its current edits
// (snapshots and the rating are not copied), opened at once.
inline void createVirtualCopy() {
  FilmStrip &f = state.film;
  if (!state.original || state.imagePath.empty() || state.dragging ||
      syncBusy())
    return;
  const std::string real = vcImageOf(state.imagePath);
  std::vector<int> used;
  size_t last =
      f.files.size(); // the last cell of this picture (it and its copies)
  for (size_t i = 0; i < f.files.size(); ++i)
    if (vcImageOf(f.files[i].path) == real) {
      last = i;
      if (f.files[i].copy > 0)
        used.push_back(f.files[i].copy);
    }
  if (last == f.files.size()) {
    setStatus("This picture is not in the filmstrip", true);
    return;
  }
  const int id = vcFreeId(used);
  if (id == 0) {
    setStatus("A picture holds at most " + std::to_string(kMaxVirtualCopies) +
                  " virtual copies",
              true);
    return;
  }
  flushSidecar(); // the picture being copied keeps its latest edits too
  const std::string path = vcPath(real, id);
  const std::string text = sidecarFileText(
      baseName(real), state.original->width, state.original->height,
      state.adjust.toJson(), PhotoMeta{});
  std::string err;
  if (!writeTextAtomic(sidecarPath(path), text, &err)) {
    setStatus("Couldn't create the virtual copy: " + err, true);
    return;
  }
  FilmEntry e;
  e.path = path;
  e.name = baseName(real);
  e.copy = id;
  e.scanned = true; // default rating, flag and label: nothing to read
  e.edited = sidecarScan(text).edited;
  if (state.image)
    e.thumb = makeThumb(*state.image); // it starts out looking like the source
  filmInsertEntry(last + 1, std::move(e));
  filmOpenIndex(static_cast<int>(last) + 1);
  setStatus("Created virtual copy " + std::to_string(id), false);
}

// Deletes the open virtual copy's edits file (the picture and its other copies
// are untouched) and goes back to the original. Asks to be chosen twice.
inline void deleteVirtualCopy() {
  FilmStrip &f = state.film;
  if (!state.original || state.imagePath.empty() || state.dragging ||
      syncBusy())
    return;
  const int n = vcCopyOf(state.imagePath);
  if (n == 0) {
    setStatus("Only a virtual copy can be deleted: this is the original", true);
    return;
  }
  if (uiState.vcDeleteArmed != state.imagePath) {
    uiState.vcDeleteArmed = state.imagePath;
    setStatus("Choose Delete Virtual Copy again to delete copy " +
                  std::to_string(n) + " and its edits",
              false);
    return;
  }
  uiState.vcDeleteArmed.clear();
  const std::string path = state.imagePath;
  std::error_code ec;
  std::filesystem::remove(sidecarPath(path), ec);
  if (ec) {
    setStatus("Couldn't delete the virtual copy: " + ec.message(), true);
    return;
  }
  // Nothing of it may be written back when the editor moves on.
  state.sidecarSaved = state.adjust.version();
  state.metaDirty = false;
  state.snapshotsDirty = false;
  const std::string real = vcImageOf(path);
  const auto it = f.byName.find(filmLower(baseName(path)));
  if (it != f.byName.end())
    filmRemoveEntry(it->second);
  const auto master = f.byName.find(filmLower(baseName(real)));
  if (master != f.byName.end())
    filmOpenIndex(static_cast<int>(master->second));
  setStatus("Deleted virtual copy " + std::to_string(n), false);
}

inline void setSelectedObjectMap(std::shared_ptr<const ObjectMap> m) {
  const size_t part = static_cast<size_t>(selPart());
  maskEditDiscrete([&](Mask &mk) {
    if (part < mk.parts.size())
      if (auto *o = std::get_if<ObjectShape>(&mk.parts[part].shape))
        o->map = m;
  });
  state.maskShowTint = true;
  state.markDirty();
}

// The "Set test map" button's selection (not a segmenter): pixels brighter than
// the picture's mean.
inline std::shared_ptr<const ObjectMap> makeTestObjectMap() {
  if (!state.original)
    return nullptr;
  const CanvasImage &im = *state.original;
  const int k = std::max(
      1, (std::max(im.width, im.height) + kObjectMapEdge - 1) / kObjectMapEdge);
  const int w = std::max(1, im.width / k), h = std::max(1, im.height / k);
  std::vector<int> luma(static_cast<size_t>(w) * h);
  double sum = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint8_t *p =
          &im.pixels[(static_cast<size_t>(y * k) * im.width + x * k) * 4];
      const int l = (54 * p[0] + 183 * p[1] + 19 * p[2] + 128) >> 8;
      luma[static_cast<size_t>(y) * w + x] = l;
      sum += l;
    }
  const double mean = sum / static_cast<double>(luma.size());
  auto m = std::make_shared<ObjectMap>();
  m->w = w;
  m->h = h;
  m->id = nextObjectMapId();
  m->a.resize(luma.size());
  for (size_t i = 0; i < luma.size(); ++i)
    m->a[i] = luma[i] > mean ? 255 : 0;
  return m;
}

inline bool selectedIsObject() {
  return selectedPartType() == MaskType::Object;
}

// The prompts a new click builds on: the ones still being segmented for this
// part, else the committed ones. Original-picture fractions.
inline std::vector<ObjectPrompt> objectCurrentPrompts(int mask, int part) {
  const MaskUi::ObjPending &p = state.mask.objPending;
  if (p.mask == mask && p.part == part)
    return p.prompts;
  const MaskSet &ms = state.adjust.masks();
  if (mask < 0 || mask >= static_cast<int>(ms.size()))
    return {};
  const Mask &m = ms[static_cast<size_t>(mask)];
  if (part < 0 || part >= static_cast<int>(m.parts.size()))
    return {};
  if (const auto *o =
          std::get_if<ObjectShape>(&m.parts[static_cast<size_t>(part)].shape))
    return o->prompts;
  return {};
}

inline void clearSelectedObject() {
  workers().segment.invalidate();
  state.mask.objPending = {};
  const size_t part = static_cast<size_t>(selPart());
  maskEditDiscrete([part](Mask &m) {
    if (part < m.parts.size())
      if (auto *o = std::get_if<ObjectShape>(&m.parts[part].shape)) {
        o->map.reset();
        o->prompts.clear();
      }
  });
}

inline void objectPrefetch() {
  if (!state.original || workers().segment.outstanding() ||
      state.mask.encodedKey == state.imageSerial)
    return;
  SegmentRequest req;
  req.image = state.original;
  req.imageKey = state.imageSerial;
  req.prefetch = true;
  workers().segment.submit(std::move(req));
  setStatus("Preparing object selection...", false);
}

// A click on the picture with an object part selected. Left click adds a
// point (foreground, or background if "Click adds Background" is on);
// right click adds the opposite kind; clicking an existing dot removes it.
inline void objectPress(float lx, float ly, bool rightClick) {
  state.openMenu = -1;
  if (!maskToolActive() || !selectedIsObject() || !state.original)
    return;
  const int mask = selMask(), part = selPart();
  const GeoFrame frame = state.adjust.geoFrame();
  const float W = state.docWidth(), H = state.docHeight();
  const float px = lx / state.zoom, py = ly / state.zoom;
  std::vector<ObjectPrompt> prompts = objectCurrentPrompts(mask, part);

  // Existing dot under the pointer -> remove it.
  const float tol = kMaskHitPx / state.zoom;
  bool removed = false;
  for (size_t k = 0; k < prompts.size(); ++k) {
    double sx, sy;
    frame.toSrc(prompts[k].x * frame.origW, prompts[k].y * frame.origH, sx, sy);
    if (std::hypot(sx / frame.srcW * W - px, sy / frame.srcH * H - py) <= tol) {
      prompts.erase(prompts.begin() + static_cast<std::ptrdiff_t>(k));
      removed = true;
      break;
    }
  }
  if (!removed) {
    double ox, oy; // view -> original
    frame.toOrig(px / W * frame.srcW, py / H * frame.srcH, ox, oy);
    if (ox < 0 || oy < 0 || ox >= frame.origW || oy >= frame.origH)
      return; // outside the picture (straighten's empty corners)
    ObjectPrompt p;
    p.x = static_cast<float>(ox / frame.origW);
    p.y = static_cast<float>(oy / frame.origH);
    p.fg = (rightClick != state.mask.objBg) ? false : true;
    // left + objBg=false -> fg; left + objBg=true -> bg;
    // right + objBg=false -> bg; right + objBg=true -> fg
    prompts.push_back(p);
  }
  if (prompts.empty()) { // last dot removed: nothing selected any more
    clearSelectedObject();
    return;
  }
  SegmentRequest req;
  req.image = state.original;
  req.imageKey = state.imageSerial;
  req.prompts = prompts;
  req.maskIdx = mask;
  req.part = part;
  state.mask.objPending = {mask, part, std::move(prompts)};
  workers().segment.submit(std::move(req));
  setStatus(state.mask.encodedKey == state.imageSerial
                ? "Selecting object..."
                : "Analyzing image (first selection takes a moment)...",
            false);
  state.markDirty();
}

// The starting shape of each kind of mask (nullopt for an unknown kind).
inline std::optional<MaskShape> newMaskShape(MaskType t) {
  switch (t) {
  case MaskType::Radial:
    // Centered on the picture as it is shown now.
    return RadialShape::fromSource(RadialShape{}, state.adjust.geoFrame());
  case MaskType::Linear:
    return LinearShape::fromSource(LinearShape{}, state.adjust.geoFrame());
  case MaskType::Brush:
    return BrushShape{}; // empty: the first drag on the image paints
  case MaskType::Luminosity:
    return LuminosityShape{}; // not spatial: nothing to place
  case MaskType::Object:
    return ObjectShape{};
  case MaskType::ColorRange:
    return ColorRangeShape{}; // nothing selected until a color is clicked
  case MaskType::Rectangle:
    // Centered on the picture as it is shown now.
    return RectShape::fromSource(RectShape{}, state.adjust.geoFrame());
  case MaskType::Polygon:
    return PolygonShape{}; // empty: clicks on the image place the vertices
  default:
    return std::nullopt;
  }
}

inline void objectPrefetch();

inline void addMask(MaskType t) {
  if (!state.original)
    return;
  MaskSet ms = state.adjust.masks();
  if (ms.size() >= kMaxMasks) {
    setStatus("Mask limit reached (" + std::to_string(kMaxMasks) + ")", true);
    return;
  }
  const auto shape = newMaskShape(t);
  if (!shape)
    return;
  Mask m;
  m.parts.push_back(MaskPart{*shape, MaskOp::Add});
  ms.push_back(std::move(m));
  state.adjust.setMasks(ms, -1);
  selectMask(static_cast<int>(ms.size()) - 1);
  state.mask.part = 0;
  // A new mask has no sliders moved yet, so no pixels change: overlay only.
  state.markDirty();

  if (t == MaskType::Object)
    objectPrefetch();
}

inline void deleteSelectedMask() {
  const int i = selMask();
  if (i < 0)
    return;
  MaskSet ms = state.adjust.masks();
  ms.erase(ms.begin() + i);
  state.adjust.setMasks(ms, -1);
  selectMask(std::min(i, static_cast<int>(ms.size()) - 1),
             /*force=*/true); // the next mask slid into index i
  state.mask.part = 0;
  updateImageFromAdjustments();
  state.markDirty();
}