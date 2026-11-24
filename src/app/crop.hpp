// app/crop.hpp: crop selection helpers
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Crop ----------------

inline bool cropToolActive() {
  return state.activeActivity == kCropActivity && state.image != nullptr;
}

// Width/height ratio the selection is locked to, or 0 for free-form.
inline float cropAspect() {
  const float ratio = kCropAspects[state.crop.aspectIdx].ratio;
  if (ratio >= 0.0f)
    return ratio;
  return (state.source && state.source->height > 0)
             ? static_cast<float>(state.source->width) /
                   static_cast<float>(state.source->height)
             : 0.0f;
}

// Shrinks the selection (about its center) to the locked aspect ratio, so
// it always stays inside where it already was.
inline void cropFitToAspect() {
  const float a = cropAspect();
  if (a <= 0.0f)
    return;
  auto &c = state.crop;
  float w = c.r - c.l, h = c.b - c.t;
  if (w <= 0.0f || h <= 0.0f)
    return;
  const float cx = (c.l + c.r) / 2.0f, cy = (c.t + c.b) / 2.0f;
  if (w / h > a)
    w = h * a;
  else
    h = w / a;
  c.l = cx - w / 2.0f;
  c.r = cx + w / 2.0f;
  c.t = cy - h / 2.0f;
  c.b = cy + h / 2.0f;
}

// Selection = the whole image being cropped (then fitted to the aspect lock).
inline void resetCropSelection() {
  auto &c = state.crop;
  c.l = 0.0f;
  c.t = 0.0f;
  c.r = state.source ? static_cast<float>(state.source->width) : 0.0f;
  c.b = state.source ? static_cast<float>(state.source->height) : 0.0f;
  c.drag = CropUi::Drag::None;
  cropFitToAspect();
}

inline void setCropAspect(size_t idx) {
  state.crop.aspectIdx = idx;
  cropFitToAspect();
  state.markDirty();
}

// Rebuilds `state.oriented` / `state.source` (and the interactive-preview
// base derived from it) when the committed crop or orientation no longer
// matches what they were built with — after applying/removing a crop, a
// rotate/flip/straighten, an undo/redo across any of them, or (force) when a
// new image is opened. Also re-seeds the selection, since the image the
// selection is measured against just changed size.
inline void syncSource(bool force) {
  if (!state.original)
    return;
  const CropRect crop = state.adjust.crop();
  const Orient orient = state.adjust.orient();
  const float angle = state.adjust.angle();
  const bool expand = state.adjust.expand();

  const bool orientChanged =
      force || !state.oriented || orient != state.orientedFor;
  // The corner mode only matters while there's an angle to straighten.
  const bool rotateChanged = orientChanged || !state.rotated ||
                             angle != state.rotatedAngle ||
                             (angle != 0.0f && expand != state.rotatedExpand);
  if (!force && state.source && !rotateChanged && crop == state.sourceCrop)
    return;

  if (orientChanged) {
    state.oriented = orientImage(state.original, orient);
    state.oriented16 = wide_chain::oriented(state.original16, orient);
    state.orientedFor = orient;
    state.orientedPreview = makePreviewBase(*state.oriented);
  }
  if (rotateChanged) {
    state.rotated = angle == 0.0f
                        ? state.oriented
                        : rotateArbitrary(*state.oriented, angle, expand);
    state.rotated16 = wide_chain::rotated(state.oriented16, angle, expand);
    state.rotatedAngle = angle;
    state.rotatedExpand = expand;
  }
  state.sourceCrop = crop;
  state.source = crop.empty() ? state.rotated : cropImage(*state.rotated, crop);
  state.source16 = wide_chain::cropped(state.rotated16, crop);
  state.previewBase = makePreviewBase(*state.source);
  state.previewBase16 =
      state.source16 ? makePreviewBase(*state.source16) : nullptr;
  state.previewImage.reset();
  resetCropSelection();
}