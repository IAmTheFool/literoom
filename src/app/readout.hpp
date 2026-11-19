// app/readout.hpp: RGB readout under the cursor
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- RGB readout under the cursor ----------------
// Both return true only when the readout changed, so LiteUI repaints only
// then (see View::onPointerMove: a change marks the canvas dirty).
static bool hoverClear() {
  const bool was = state.hoverValid;
  state.hoverValid = false;
  return was;
}

// Samples the picture the canvas is showing: the original in Before (or the
// left half of Split), else the edited image / mid-drag preview.
static bool hoverSample(float lx, float ly) {
  if (state.dragging)
    return false; // a drag is running: leave the readout alone
  const float docW = state.docWidth(), docH = state.docHeight();
  if (!state.image || docW <= 0.0f || docH <= 0.0f || state.zoom <= 0.0f)
    return hoverClear();
  const float dx = lx / state.zoom, dy = ly / state.zoom;
  if (dx < 0.0f || dy < 0.0f || dx >= docW || dy >= docH)
    return hoverClear();
  const bool before =
      state.compare == CompareMode::Before ||
      (state.compare == CompareMode::Split && dx < docW * state.splitPos);
  const CanvasImage *img = (before && state.source)
                               ? state.source.get()
                               : (state.showPreview() ? state.previewImage.get()
                                                      : state.image.get());
  if (!img || img->width <= 0 || img->height <= 0)
    return hoverClear();
  const int ix =
      std::clamp(static_cast<int>(dx / docW * img->width), 0, img->width - 1);
  const int iy =
      std::clamp(static_cast<int>(dy / docH * img->height), 0, img->height - 1);
  const uint8_t *px =
      img->pixels.data() + (static_cast<size_t>(iy) * img->width + ix) * 4;
  if (px[3] == 0)
    return hoverClear(); // straighten's empty corner
  const bool changed = !state.hoverValid || state.hoverR != px[0] ||
                       state.hoverG != px[1] || state.hoverB != px[2];
  state.hoverValid = true;
  state.hoverR = px[0];
  state.hoverG = px[1];
  state.hoverB = px[2];
  return changed;
}

