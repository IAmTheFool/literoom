// app/rotate.hpp: rotate / flip and straighten
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Rotate / flip ----------------

// One undo step. `state.rotated` is what crops are measured against, so its
// size is the (W, H) geoStep needs.
inline void applyGeoOp(GeoOp op) {
  if (!state.original || !state.rotated || geometryBusy())
    return;
  Orient o = state.adjust.orient();
  float angle = state.adjust.angle();
  CropRect c = state.adjust.crop();
  int W = state.rotated->width, H = state.rotated->height;
  geoStep(o, angle, c, W, H, op);
  state.adjust.setGeometry(o, angle, c);
  updateImageFromAdjustments(); // syncSource() rebuilds oriented/rotated/source
}

// Undoes every 90-degree turn and flip in one step (the crop follows,
// staying on the same part of the picture; the straighten angle is kept,
// still tilting the picture the same way relative to its own edges).
inline void resetOrientation() {
  if (!state.original || !state.rotated || geometryBusy() ||
      state.adjust.orient().identity())
    return;
  Orient o = state.adjust.orient();
  float angle = state.adjust.angle();
  CropRect c = state.adjust.crop();
  int W = state.rotated->width, H = state.rotated->height;
  if (o.flip)
    geoStep(o, angle, c, W, H, GeoOp::FlipH); // leaves a pure rotation
  while (o.rot != 0)
    geoStep(o, angle, c, W, H, GeoOp::RotateCCW);
  state.adjust.setGeometry(o, angle, c);
  updateImageFromAdjustments();
}

// ---------------- Straighten (arbitrary-angle rotation) ----------------

// Slider drag: one undo step per drag. Only records the value — the
// resample is deferred to canvasDirtySource (see there), once per frame.
inline void setAngleFromSlider(float deg) {
  state.adjust.setAngle(snapAngle(deg), /*coalesce=*/true);
}

// A discrete change (nudge button / reset): its own undo step, resampled at
// full resolution on the next frame.
inline void setAngleDiscrete(float deg) {
  if (!state.original)
    return;
  state.adjust.setAngle(snapAngle(deg), /*coalesce=*/false);
  state.markAdjustDirty();
}
inline void nudgeAngle(float delta) {
  setAngleDiscrete(state.adjust.angle() + delta);
}

inline void setCornerMode(bool expand) {
  if (!state.original)
    return;
  state.adjust.setExpand(expand);
  state.markAdjustDirty();
}

// Interactive path for an angle drag on a large image: turns the small
// `orientedPreview` instead of the full-resolution picture, drawn stretched
// to the size the full-resolution result will have (previewDoc*). The
// full-resolution turn runs once, when the drag ends (pointer up / cancel).
inline void updateAnglePreview() {
  if (!state.orientedPreview || !state.oriented)
    return;
  const float a = state.adjust.angle();
  const bool ex = state.adjust.expand();
  auto turned = rotateArbitrary(*state.orientedPreview, a, ex);
  state.previewImage = renderAdjusted(state.adjust, *turned);
  state.histStale = true;
  const Dims d =
      rotatedDims(state.oriented->width, state.oriented->height, a, ex);
  state.previewDocW = static_cast<float>(d.w);
  state.previewDocH = static_cast<float>(d.h);
  state.markDirty();
}

inline void undo() {
  if (!state.adjust.canUndo())
    return;
  state.adjust.undo();
  uiState.dropCurveSelections(); // the curves may be different ones now
  updateImageFromAdjustments();
}

inline void redo() {
  if (!state.adjust.canRedo())
    return;
  state.adjust.redo();
  uiState.dropCurveSelections();
  updateImageFromAdjustments();
}

