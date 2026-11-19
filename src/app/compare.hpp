// app/compare.hpp: Before / After (fence: compare)
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// BEGIN compare
// ---------------- Before / After ----------------
// "Before" is state.source: the picture with its crop and rotation but none
// of the slider, curve, color or mask edits. Keeping the geometry means the
// canvas doesn't change size, so the comparison is pixel for pixel.
static void toggleBefore() {
  if (!state.image)
    return;
  state.compare = state.compare == CompareMode::Before ? CompareMode::Off
                                                       : CompareMode::Before;
  state.markDirty();
}
static void toggleSplit() {
  if (!state.image)
    return;
  state.compare = state.compare == CompareMode::Split ? CompareMode::Off
                                                      : CompareMode::Split;
  state.markDirty();
}
// Dragging in the canvas moves the split line.
static void splitFromX(float localX) {
  const float w = state.docWidth() * state.zoom;
  if (w <= 0.0f)
    return;
  state.splitPos = std::clamp(localX / w, 0.02f, 0.98f);
  state.markDirty();
}

// A small dark pill with `text`, its left (or, if alignRight, right) edge at x.
static void paintCompareLabel(CanvasContext &ctx, const std::string &text,
                              float x, float y, bool alignRight) {
  ctx.setFont("", 12.0f, FontWeight::SemiBold);
  const float w = ctx.measureText(text).width + 16.0f, h = 22.0f;
  const float left = alignRight ? x - w : x;
  ctx.setFillColor(Color{0, 0, 0, 160});
  ctx.beginPath();
  ctx.roundRect(left, y, w, h, 4.0f);
  ctx.fill();
  ctx.setFillColor(kColorWhite);
  ctx.setTextAlign(TextAlign::Start);
  ctx.setTextBaseline(TextBaseline::Middle);
  ctx.fillText(text, left + 8.0f, y + h / 2.0f);
}

// Screen-space decoration over the canvas: labels and the split line.
static void paintCompareOverlay(CanvasContext &ctx) {
  const float W = state.docWidth() * state.zoom;
  const float H = state.docHeight() * state.zoom;
  if (state.compare == CompareMode::Before) {
    paintCompareLabel(ctx, "Before", 10.0f, 10.0f, false);
    return;
  }
  const float x = std::floor(W * state.splitPos);
  ctx.setStrokeColor(kColorWhite);
  ctx.setLineWidth(2.0f);
  ctx.beginPath();
  ctx.moveTo(x, 0);
  ctx.lineTo(x, H);
  ctx.stroke();
  paintCompareLabel(ctx, "Before", 10.0f, 10.0f, false);
  paintCompareLabel(ctx, "After", W - 10.0f, 10.0f, true);
}
// END compare

