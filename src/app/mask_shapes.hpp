// app/mask_shapes.hpp: radial, linear and rectangle mask geometry on the canvas
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---- Radial mask geometry on the canvas (document pixels) ----
struct MaskGeom {
  float cx, cy; // center
  float rx, ry; // half-axes
  float ang;    // radians, clockwise
  float c, s;   // cos, sin of ang
  float S;      // the shorter document side (radius unit)
};

static MaskGeom radialGeom(const RadialShape &r) {
  MaskGeom g{};
  const float W = state.docWidth(), H = state.docHeight();
  g.S = std::min(W, H);
  g.cx = r.cx * W;
  g.cy = r.cy * H;
  g.rx = r.rx * g.S;
  g.ry = r.ry * g.S;
  g.ang = r.angle * (3.14159265358979f / 180.0f);
  g.c = std::cos(g.ang);
  g.s = std::sin(g.ang);
  return g;
}

enum {
  kMaskHitNone = 0,
  kMaskHitBody,
  kMaskHitX,
  kMaskHitY,
  kMaskHitLinEnd,
  kMaskHitLinStart
};

// What is under (px, py)? Handles (only for the selected mask) win over the
// body. The four handles sit on the rotated axes at the ellipse outline.
static int maskHit(const RadialShape &r, float px, float py, bool handles) {
  const MaskGeom g = radialGeom(r);
  const float tol = kMaskHitPx / state.zoom;
  if (handles) {
    const float hx[2] = {g.cx + g.rx * g.c, g.cx - g.rx * g.c};
    const float hy[2] = {g.cy + g.rx * g.s, g.cy - g.rx * g.s};
    const float vx[2] = {g.cx - g.ry * g.s, g.cx + g.ry * g.s};
    const float vy[2] = {g.cy + g.ry * g.c, g.cy - g.ry * g.c};
    for (int k = 0; k < 2; ++k) {
      if (std::hypot(px - hx[k], py - hy[k]) <= tol)
        return kMaskHitX;
      if (std::hypot(px - vx[k], py - vy[k]) <= tol)
        return kMaskHitY;
    }
  }
  const float dx = px - g.cx, dy = py - g.cy;
  const float u = (dx * g.c + dy * g.s) / g.rx;
  const float v = (-dx * g.s + dy * g.c) / g.ry;
  return (u * u + v * v <= 1.0f) ? kMaskHitBody : kMaskHitNone;
}

// ---- Linear mask geometry on the canvas (document pixels) ----
struct LinGeom {
  float cx, cy; // center
  float ux, uy; // unit direction of fade-out
  float len;    // half length of the ramp
  float ang;    // radians
  float S;      // the shorter document side
  // The weight-1 line's and the weight-0 line's axis points.
  float sx() const { return cx - ux * len; }
  float sy() const { return cy - uy * len; }
  float ex() const { return cx + ux * len; }
  float ey() const { return cy + uy * len; }
};

static LinGeom linearGeom(const LinearShape &l) {
  LinGeom g{};
  const float W = state.docWidth(), H = state.docHeight();
  g.S = std::min(W, H);
  g.cx = l.cx * W;
  g.cy = l.cy * H;
  g.len = std::max(l.len, kMaskMinRadius) * g.S;
  g.ang = l.angle * (3.14159265358979f / 180.0f);
  g.ux = std::cos(g.ang);
  g.uy = std::sin(g.ang);
  return g;
}

// Handles (selected mask only) sit where the axis crosses the two lines; the
// body is the band between the lines.
static int linearHit(const LinearShape &l, float px, float py, bool handles) {
  const LinGeom g = linearGeom(l);
  const float tol = kMaskHitPx / state.zoom;
  if (handles) {
    if (std::hypot(px - g.ex(), py - g.ey()) <= tol)
      return kMaskHitLinEnd;
    if (std::hypot(px - g.sx(), py - g.sy()) <= tol)
      return kMaskHitLinStart;
  }
  const float s = (px - g.cx) * g.ux + (py - g.cy) * g.uy;
  return std::fabs(s) <= g.len ? kMaskHitBody : kMaskHitNone;
}

// ---- Rectangle mask geometry on the canvas (document pixels) ----
// Same layout as the radial mask: a center, half-sizes along the rotated axes,
// and four handles on the axes (here at the middle of each side).
static MaskGeom rectGeom(const RectShape &r) {
  MaskGeom g{};
  const float W = state.docWidth(), H = state.docHeight();
  g.S = std::min(W, H);
  g.cx = r.cx * W;
  g.cy = r.cy * H;
  g.rx = std::max(r.rx, kMaskMinRadius) * g.S;
  g.ry = std::max(r.ry, kMaskMinRadius) * g.S;
  g.ang = r.angle * (3.14159265358979f / 180.0f);
  g.c = std::cos(g.ang);
  g.s = std::sin(g.ang);
  return g;
}

// Handles (selected part only) win over the body, which is the rectangle.
static int rectHit(const RectShape &r, float px, float py, bool handles) {
  const MaskGeom g = rectGeom(r);
  const float tol = kMaskHitPx / state.zoom;
  if (handles) {
    const float hx[2] = {g.cx + g.rx * g.c, g.cx - g.rx * g.c};
    const float hy[2] = {g.cy + g.rx * g.s, g.cy - g.rx * g.s};
    const float vx[2] = {g.cx - g.ry * g.s, g.cx + g.ry * g.s};
    const float vy[2] = {g.cy + g.ry * g.c, g.cy - g.ry * g.c};
    for (int k = 0; k < 2; ++k) {
      if (std::hypot(px - hx[k], py - hy[k]) <= tol)
        return kMaskHitX;
      if (std::hypot(px - vx[k], py - vy[k]) <= tol)
        return kMaskHitY;
    }
  }
  const float dx = px - g.cx, dy = py - g.cy;
  const float u = (dx * g.c + dy * g.s) / g.rx;
  const float v = (-dx * g.s + dy * g.c) / g.ry;
  return (std::fabs(u) <= 1.0f && std::fabs(v) <= 1.0f) ? kMaskHitBody
                                                        : kMaskHitNone;
}

