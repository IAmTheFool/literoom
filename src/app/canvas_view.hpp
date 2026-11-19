// app/canvas_view.hpp: where the picture sits inside the canvas: zoom, scroll, scrollbars
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---- The canvas view: where the picture sits inside the canvas ----
// The picture is state.docWidth() x docHeight() image pixels shown at
// state.zoom logical units per pixel, with kViewPad of margin all round. If it
// fits an axis it is centered there; otherwise that axis scrolls.
using ViewDrag = decltype(EditorState::view)::Drag;
constexpr float kViewPad = 24.0f;
constexpr float kViewBar = kScrollbarThickness;
constexpr float kViewMinThumb = 24.0f;

struct ViewGeo {
  float W = 0, H = 0;           // picture size on screen
  float cw = 0, ch = 0;         // padded content size
  bool hbar = false, vbar = false;
  float baseX = 0, baseY = 0;   // picture origin in the canvas at scroll 0
  float maxX = 0, maxY = 0;     // largest scroll offset
};

// Geometry of the view; also keeps the scroll offsets inside their range
// (the picture can change size under us: crop, rotate, a new zoom).
static ViewGeo viewGeo(bool clampScroll = true) {
  auto &v = state.view;
  ViewGeo g;
  g.W = state.docWidth() * state.zoom;
  g.H = state.docHeight() * state.zoom;
  g.cw = g.W + 2.0f * kViewPad;
  g.ch = g.H + 2.0f * kViewPad;
  g.hbar = g.cw > v.w + 0.5f;
  g.vbar = g.ch > v.h + 0.5f;
  g.maxX = std::max(0.0f, g.cw - v.w);
  g.maxY = std::max(0.0f, g.ch - v.h);
  g.baseX = g.hbar ? kViewPad : (v.w - g.W) / 2.0f;
  g.baseY = g.vbar ? kViewPad : (v.h - g.H) / 2.0f;
  if (clampScroll) {
    v.scrollX = std::clamp(v.scrollX, 0.0f, g.maxX);
    v.scrollY = std::clamp(v.scrollY, 0.0f, g.maxY);
  }
  return g;
}

// The picture's top-left corner in canvas coordinates, on a whole device pixel
// so a 100% picture is drawn pixel-exact at any display scale.
static void viewOrigin(const ViewGeo &g, float &ox, float &oy) {
  const float d = state.dpr;
  ox = std::round((g.baseX - state.view.scrollX) * d) / d;
  oy = std::round((g.baseY - state.view.scrollY) * d) / d;
}
// Canvas coordinates -> picture-local coordinates (what the crop, mask, picker
// and readout code take: divide by state.zoom to get image pixels).
static void viewLocal(float x, float y, float &lx, float &ly) {
  float ox, oy;
  viewOrigin(viewGeo(), ox, oy);
  lx = x - ox;
  ly = y - oy;
}

// Changes the display scale and/or the device-pixel zoom, keeping the image
// point under canvas position (ax, ay) where it is.
static void changeView(float newDpr, float newZoomDev, float ax, float ay) {
  auto &v = state.view;
  const bool have = state.image && state.zoom > 0.0f;
  float ix = 0, iy = 0; // image-pixel point under the anchor
  if (have) {
    const ViewGeo g = viewGeo();
    ix = (ax - (g.baseX - v.scrollX)) / state.zoom;
    iy = (ay - (g.baseY - v.scrollY)) / state.zoom;
  }
  state.dpr = std::max(1.0f, newDpr);
  applyZoomDev(newZoomDev);
  if (have) {
    const ViewGeo g = viewGeo(false);
    v.scrollX = g.baseX + ix * state.zoom - ax;
    v.scrollY = g.baseY + iy * state.zoom - ay;
  }
  viewGeo(); // clamp
  state.markDirty();
}
static void zoomAbout(float zoomDev, float ax, float ay) {
  changeView(state.dpr, zoomDev, ax, ay);
}
// Zooms about the middle of the canvas (menus, buttons, shortcuts).
static void setZoom(float zoomDev) {
  zoomAbout(zoomDev, state.view.w / 2.0f, state.view.h / 2.0f);
}
// The display scale changed (first known at startup, or the window moved to
// another monitor): keep the same device zoom, so 100% stays 100%.
static void setDpr(float dpr) {
  changeView(dpr, state.zoomDev, state.view.w / 2.0f, state.view.h / 2.0f);
}

// Scrollbar thumb along a track of length `track`.
struct ViewThumb {
  float pos, len;
};
static ViewThumb viewThumb(float track, float content, float scroll,
                           float maxScroll) {
  const float len =
      std::clamp(track * track / std::max(1.0f, content), kViewMinThumb, track);
  const float pos = maxScroll > 0 ? scroll / maxScroll * (track - len) : 0.0f;
  return {pos, len};
}

// A press at canvas position (x, y): starts a scrollbar drag, or pages the
// view when it lands on a track. Returns true if it landed on a bar.
static bool viewBarPress(float x, float y) {
  if (!state.image)
    return false;
  auto &v = state.view;
  const ViewGeo g = viewGeo();
  const float availW = v.w - (g.vbar ? kViewBar : 0.0f);
  const float availH = v.h - (g.hbar ? kViewBar : 0.0f);
  if (g.vbar && x >= v.w - kViewBar && y < availH) {
    const ViewThumb t = viewThumb(availH, g.ch, v.scrollY, g.maxY);
    if (y >= t.pos && y <= t.pos + t.len) {
      v.drag = ViewDrag::VThumb;
      v.dragY = y;
      v.dragScrollY = v.scrollY;
    } else {
      v.scrollY += (y < t.pos ? -availH : availH);
      viewGeo();
      state.markDirty();
    }
    return true;
  }
  if (g.hbar && y >= v.h - kViewBar && x < availW) {
    const ViewThumb t = viewThumb(availW, g.cw, v.scrollX, g.maxX);
    if (x >= t.pos && x <= t.pos + t.len) {
      v.drag = ViewDrag::HThumb;
      v.dragX = x;
      v.dragScrollX = v.scrollX;
    } else {
      v.scrollX += (x < t.pos ? -availW : availW);
      viewGeo();
      state.markDirty();
    }
    return true;
  }
  return false;
}
static void viewPanBegin(float x, float y) {
  auto &v = state.view;
  v.drag = ViewDrag::Pan;
  v.dragX = x;
  v.dragY = y;
  v.dragScrollX = v.scrollX;
  v.dragScrollY = v.scrollY;
}
static void viewDragTo(float x, float y) {
  auto &v = state.view;
  if (!state.image || v.drag == ViewDrag::None)
    return;
  const ViewGeo g = viewGeo();
  const float availW = v.w - (g.vbar ? kViewBar : 0.0f);
  const float availH = v.h - (g.hbar ? kViewBar : 0.0f);
  switch (v.drag) {
  case ViewDrag::Pan:
    v.scrollX = v.dragScrollX - (x - v.dragX);
    v.scrollY = v.dragScrollY - (y - v.dragY);
    break;
  case ViewDrag::VThumb: {
    const ViewThumb t = viewThumb(availH, g.ch, v.scrollY, g.maxY);
    const float range = availH - t.len;
    if (range > 0)
      v.scrollY = v.dragScrollY + (y - v.dragY) / range * g.maxY;
    break;
  }
  case ViewDrag::HThumb: {
    const ViewThumb t = viewThumb(availW, g.cw, v.scrollX, g.maxX);
    const float range = availW - t.len;
    if (range > 0)
      v.scrollX = v.dragScrollX + (x - v.dragX) / range * g.maxX;
    break;
  }
  default:
    break;
  }
  viewGeo(); // clamp
  state.markDirty();
}
static void paintViewBars(CanvasContext &ctx, const ViewGeo &g) {
  auto &v = state.view;
  const float availW = v.w - (g.vbar ? kViewBar : 0.0f);
  const float availH = v.h - (g.hbar ? kViewBar : 0.0f);
  if (g.vbar) {
    const ViewThumb t = viewThumb(availH, g.ch, v.scrollY, g.maxY);
    ctx.setFillColor(kColorTrack);
    ctx.fillRect(v.w - kViewBar, 0, kViewBar, availH);
    ctx.setFillColor(kColorScrollThumb);
    ctx.beginPath();
    ctx.roundRect(v.w - kViewBar + 2, t.pos + 1, kViewBar - 4, t.len - 2, 3);
    ctx.fill();
  }
  if (g.hbar) {
    const ViewThumb t = viewThumb(availW, g.cw, v.scrollX, g.maxX);
    ctx.setFillColor(kColorTrack);
    ctx.fillRect(0, v.h - kViewBar, availW, kViewBar);
    ctx.setFillColor(kColorScrollThumb);
    ctx.beginPath();
    ctx.roundRect(t.pos + 1, v.h - kViewBar + 2, t.len - 2, kViewBar - 4, 3);
    ctx.fill();
  }
  if (g.vbar && g.hbar) {
    ctx.setFillColor(kColorTrack);
    ctx.fillRect(v.w - kViewBar, v.h - kViewBar, kViewBar, kViewBar);
  }
}

static void syncSource(bool force);

static void updatePreviewFromAdjustments();

// Defined after GeometryWorker (needs its type); forward-declared here
// because updateImageFromAdjustments (right below) is written once, in its
// natural place, even though it now has to decide between the synchronous
// pipeline and GeometryWorker before either exists yet in the file.
static bool geometryDirty();
static void submitGeometryJob();
static void announceColorTag();

