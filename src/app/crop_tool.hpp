// app/crop_tool.hpp: crop tool: hit-testing, dragging, apply / remove crop
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// Which part of the selection is under (px, py) (image coordinates)?
// Returns kEdge* bits for a handle, kEdgeMove for the interior, 0 for none.
inline int cropHitTest(float px, float py) {
  const auto &c = state.crop;
  const float tol = kCropHitPx / state.zoom;
  const bool inX = px >= c.l - tol && px <= c.r + tol;
  const bool inY = py >= c.t - tol && py <= c.b + tol;
  int m = 0;
  if (inY) {
    if (std::abs(px - c.l) <= tol)
      m |= kEdgeL;
    else if (std::abs(px - c.r) <= tol)
      m |= kEdgeR;
  }
  if (inX) {
    if (std::abs(py - c.t) <= tol)
      m |= kEdgeT;
    else if (std::abs(py - c.b) <= tol)
      m |= kEdgeB;
  }
  if (m)
    return m;
  if (px > c.l && px < c.r && py > c.t && py < c.b)
    return kEdgeMove;
  return 0;
}

// Sets the selection to the rectangle spanned by a fixed corner (ax, ay)
// and the pointer, honoring the aspect lock and the image bounds. Used for
// both "drag out a new selection" and "drag a corner handle".
inline void cropDragCorner(float ax, float ay, float px, float py) {
  auto &c = state.crop;
  const float W = state.docWidth(), H = state.docHeight();
  const float a = cropAspect();
  const float sx = px >= ax ? 1.0f : -1.0f;
  const float sy = py >= ay ? 1.0f : -1.0f;
  float w = std::abs(px - ax), h = std::abs(py - ay);
  if (a > 0.0f) {
    w = std::max(w, h * a); // grow to whichever dimension asks for more
    h = w / a;
    const float maxW = sx > 0.0f ? W - ax : ax;
    const float maxH = sy > 0.0f ? H - ay : ay;
    if (w > maxW) {
      w = maxW;
      h = w / a;
    }
    if (h > maxH) {
      h = maxH;
      w = h * a;
    }
  }
  const float x2 = ax + sx * w, y2 = ay + sy * h;
  c.l = std::min(ax, x2);
  c.r = std::max(ax, x2);
  c.t = std::min(ay, y2);
  c.b = std::max(ay, y2);
}

// Drags one edge or corner of the existing selection to the pointer.
inline void cropResize(int mask, float px, float py) {
  auto &c = state.crop;
  const float W = state.docWidth(), H = state.docHeight();
  const float minSz = std::min({kCropMinPx, W, H});
  const bool horiz = (mask & (kEdgeL | kEdgeR)) != 0;
  const bool vert = (mask & (kEdgeT | kEdgeB)) != 0;

  if (horiz && vert) { // corner: the opposite corner stays put
    cropDragCorner((mask & kEdgeL) ? c.r : c.l, (mask & kEdgeT) ? c.b : c.t, px,
                   py);
    return;
  }

  const float a = cropAspect();
  if (a <= 0.0f) { // free-form edge
    if (mask & kEdgeL)
      c.l = std::clamp(px, 0.0f, c.r - minSz);
    if (mask & kEdgeR)
      c.r = std::clamp(px, c.l + minSz, W);
    if (mask & kEdgeT)
      c.t = std::clamp(py, 0.0f, c.b - minSz);
    if (mask & kEdgeB)
      c.b = std::clamp(py, c.t + minSz, H);
    return;
  }

  // Aspect-locked edge: the dragged edge sets the primary dimension, and
  // the other dimension follows, centered on where the selection was.
  if (horiz) {
    const float cy = (c.t + c.b) / 2.0f;
    const bool left = (mask & kEdgeL) != 0;
    const float room = left ? c.r : W - c.l;
    const float roomByH = 2.0f * std::min(cy, H - cy) * a;
    float w = left ? c.r - px : px - c.l;
    w = std::clamp(w, minSz, std::max(minSz, std::min(room, roomByH)));
    const float h = w / a;
    if (left)
      c.l = c.r - w;
    else
      c.r = c.l + w;
    c.t = cy - h / 2.0f;
    c.b = cy + h / 2.0f;
  } else {
    const float cx = (c.l + c.r) / 2.0f;
    const bool top = (mask & kEdgeT) != 0;
    const float room = top ? c.b : H - c.t;
    const float roomByW = 2.0f * std::min(cx, W - cx) / a;
    float h = top ? c.b - py : py - c.t;
    h = std::clamp(h, minSz, std::max(minSz, std::min(room, roomByW)));
    const float w = h * a;
    if (top)
      c.t = c.b - h;
    else
      c.b = c.t + h;
    c.l = cx - w / 2.0f;
    c.r = cx + w / 2.0f;
  }
}

// Canvas mouse handlers. (lx, ly) are canvas-local screen px; dividing by
// the zoom gives image pixels.
inline void cropPress(float lx, float ly) {
  state.openMenu = -1; // the canvas now swallows presses that used to fall
                       // through to the workspace's "close menu" handler
  if (!cropToolActive())
    return;
  auto &c = state.crop;
  const float px = std::clamp(lx / state.zoom, 0.0f, state.docWidth());
  const float py = std::clamp(ly / state.zoom, 0.0f, state.docHeight());
  const int hit = cropHitTest(px, py);
  if (hit == kEdgeMove) {
    c.drag = CropUi::Drag::Move;
    c.grabX = px - c.l;
    c.grabY = py - c.t;
  } else if (hit) {
    c.drag = CropUi::Drag::Resize;
    c.mask = hit;
  } else {
    // Empty area: start a new selection here. The old one stays until the
    // pointer actually moves, so a plain click doesn't wipe it out.
    c.drag = CropUi::Drag::New;
    c.ax = px;
    c.ay = py;
  }
}

inline void cropDrag(float lx, float ly) {
  auto &c = state.crop;
  if (!cropToolActive() || c.drag == CropUi::Drag::None)
    return;
  const float W = state.docWidth(), H = state.docHeight();
  const float px = std::clamp(lx / state.zoom, 0.0f, W);
  const float py = std::clamp(ly / state.zoom, 0.0f, H);
  switch (c.drag) {
  case CropUi::Drag::New:
    cropDragCorner(c.ax, c.ay, px, py);
    break;
  case CropUi::Drag::Resize:
    cropResize(c.mask, px, py);
    break;
  case CropUi::Drag::Move: {
    const float w = c.r - c.l, h = c.b - c.t;
    c.l = std::clamp(px - c.grabX, 0.0f, W - w);
    c.t = std::clamp(py - c.grabY, 0.0f, H - h);
    c.r = c.l + w;
    c.b = c.t + h;
    break;
  }
  case CropUi::Drag::None:
    break;
  }
  state.markDirty();
}

// Commits the current selection as the crop (one undo step).
inline void applyCrop() {
  if (!state.source || geometryBusy())
    return;
  const auto &c = state.crop;
  const int sw = state.source->width, sh = state.source->height;
  const int x0 = std::clamp(static_cast<int>(std::lround(c.l)), 0, sw);
  const int y0 = std::clamp(static_cast<int>(std::lround(c.t)), 0, sh);
  const int x1 = std::clamp(static_cast<int>(std::lround(c.r)), x0, sw);
  const int y1 = std::clamp(static_cast<int>(std::lround(c.b)), y0, sh);
  const int w = x1 - x0, h = y1 - y0;
  if (w < 1 || h < 1)
    return;
  if (w == sw && h == sh)
    return; // selection covers everything: nothing to crop
  // The selection is relative to the already-cropped `source`; the stored
  // crop is relative to the untouched original, so add the existing offset.
  const CropRect base = state.adjust.crop(); // 0,0 when there's no crop yet
  state.adjust.setCrop(CropRect{base.x + x0, base.y + y0, w, h});
  updateImageFromAdjustments(); // syncSource() rebuilds source + selection
}

// Drops the committed crop, restoring the full original (one undo step).
inline void removeCrop() {
  if (geometryBusy() || state.adjust.crop().empty())
    return;
  state.adjust.setCrop(CropRect{});
  updateImageFromAdjustments();
}