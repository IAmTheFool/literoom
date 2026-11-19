// app/mask_polygon.hpp: polygon / lasso tool
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---- Polygon / lasso tool ----
// With a polygon part selected every press edits its outline (like a brush,
// pick another part from the list):
//   click empty space    adds a vertex (on a closed outline: into the nearest
//                        side), and dragging before release places it
//   press on a vertex    drags it; right-click on it removes it
//   press on a side      inserts a vertex there and drags it
//   press inside         moves the whole polygon
// With "Freehand lasso" on, a press starts a new outline and the drag draws it
// (simplified while you draw). The outline is always closed.
constexpr float kPolyClampLo = -0.25f; // vertices stay near the picture
constexpr float kPolyClampHi = 1.25f;
constexpr float kLassoTolPx = 1.5f; // simplification tolerance, screen px

static float polyClampCoord(float v) {
  return std::clamp(v, kPolyClampLo, kPolyClampHi);
}
// The polygon's vertices in document pixels (what the pointer is measured in).
static std::vector<PolyPt> polyDocPts(const PolygonShape &view) {
  const float W = state.docWidth(), H = state.docHeight();
  std::vector<PolyPt> out;
  out.reserve(view.pts.size());
  for (const PolyPt &p : view.pts)
    out.push_back(PolyPt{p.x * W, p.y * H});
  return out;
}
// The vertex under (px, py), the closest one within the grab tolerance, or -1.
static int polyVertexAt(const std::vector<PolyPt> &dp, float px, float py) {
  const float tol = kMaskHitPx / state.zoom;
  int best = -1;
  float bestD = tol;
  for (size_t i = 0; i < dp.size(); ++i) {
    const float d = std::hypot(px - dp[i].x, py - dp[i].y);
    if (d <= bestD) {
      bestD = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}
// What picks an UNSELECTED polygon: a vertex, or inside a closed outline.
static bool polygonBodyHit(const PolygonShape &view, float px, float py) {
  const std::vector<PolyPt> dp = polyDocPts(view);
  return polyVertexAt(dp, px, py) >= 0 || polyContains(dp, px, py);
}

static void
polygonEditPts(int mask, size_t part, int control,
               const std::function<void(std::vector<PolyPt> &)> &fn) {
  const GeoFrame frame = state.adjust.geoFrame();
  maskEdit(
      mask,
      [&](Mask &m) {
        editShapeInView<PolygonShape>(m, part, frame,
                                      [&](PolygonShape &s) { fn(s.pts); });
      },
      control);
}

static void polygonDirty() {
  enterDrag(false); // recompute on the cheap preview path while held
  state.markAdjustDirty();
  state.markDirty();
}

static void polygonBegin(float px, float py) {
  const int i = selMask();
  if (i < 0)
    return;
  const size_t part = static_cast<size_t>(selPart());
  const MaskSet &ms = state.adjust.masks();
  if (part >= ms[static_cast<size_t>(i)].parts.size())
    return;
  const GeoFrame frame = state.adjust.geoFrame();
  const auto view =
      polygonInView(ms[static_cast<size_t>(i)].parts[part], frame);
  if (!view)
    return;
  const float W = state.docWidth(), H = state.docHeight();
  if (W <= 0 || H <= 0)
    return;
  MaskUi &u = state.mask;
  const std::vector<PolyPt> dp = polyDocPts(*view);
  const PolyPt here{polyClampCoord(px / W), polyClampCoord(py / H)};

  if (u.polyLasso) { // a new outline, drawn by dragging
    u.lassoRaw.clear();
    u.lassoRaw.push_back(PolyPt{px, py});
    polygonEditPts(i, part, kMaskCtlPolyLasso,
                   [&](std::vector<PolyPt> &pts) { pts.assign(1, here); });
    u.drag = MaskUi::Drag::Lasso;
    polygonDirty();
    return;
  }

  const int vi = polyVertexAt(dp, px, py);
  if (vi >= 0) { // grab a vertex
    u.drag = MaskUi::Drag::PolyVertex;
    u.polyIdx = vi;
    state.markDirty();
    return;
  }

  // Where would a new vertex go? Into the nearest side of a closed outline,
  // else at the end.
  int edge = -1;
  float qx = 0, qy = 0;
  const float dist = polyNearestEdge(dp, px, py, &edge, &qx, &qy);
  const bool closed = dp.size() >= 3;
  const float tol = kMaskHitPx / state.zoom;
  const auto insertAt = [&](size_t at, PolyPt p) {
    bool full = false;
    polygonEditPts(i, part, kMaskCtlPolyVertex, [&](std::vector<PolyPt> &pts) {
      if (pts.size() >= kPolyMaxPoints) {
        full = true;
        return;
      }
      pts.insert(pts.begin() +
                     static_cast<std::ptrdiff_t>(std::min(at, pts.size())),
                 p);
    });
    if (full) {
      setStatus("Polygon point limit reached (" +
                    std::to_string(kPolyMaxPoints) + ")",
                true);
      return;
    }
    u.drag = MaskUi::Drag::PolyVertex;
    u.polyIdx = static_cast<int>(at);
    polygonDirty();
  };
  if (closed && edge >= 0 && dist <= tol) { // on a side: split it there
    insertAt(static_cast<size_t>(edge) + 1,
             PolyPt{polyClampCoord(qx / W), polyClampCoord(qy / H)});
    return;
  }
  if (closed && polyContains(dp, px, py)) { // inside: move the whole polygon
    u.drag = MaskUi::Drag::PolyMove;
    u.lastX = px;
    u.lastY = py;
    state.markDirty();
    return;
  }
  if (closed && edge >= 0)
    insertAt(static_cast<size_t>(edge) + 1, here); // outside: grow toward it
  else
    insertAt(view->pts.size(), here); // fewer than 3 points: append
}

static void polygonDragTo(float px, float py, MaskUi::Drag d) {
  const int i = selMask();
  if (i < 0)
    return;
  MaskUi &u = state.mask;
  const size_t part = static_cast<size_t>(selPart());
  const float W = state.docWidth(), H = state.docHeight();
  if (W <= 0 || H <= 0)
    return;
  if (d == MaskUi::Drag::PolyVertex) {
    const size_t k = static_cast<size_t>(std::max(u.polyIdx, 0));
    const PolyPt p{polyClampCoord(px / W), polyClampCoord(py / H)};
    polygonEditPts(i, part, kMaskCtlPolyVertex, [&](std::vector<PolyPt> &pts) {
      if (k < pts.size())
        pts[k] = p;
    });
  } else if (d == MaskUi::Drag::PolyMove) {
    float dx = (px - u.lastX) / W, dy = (py - u.lastY) / H;
    polygonEditPts(i, part, kMaskCtlPolyMove, [&](std::vector<PolyPt> &pts) {
      if (pts.empty())
        return;
      float x0 = pts[0].x, x1 = x0, y0 = pts[0].y, y1 = y0;
      for (const PolyPt &p : pts) {
        x0 = std::min(x0, p.x);
        x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y);
        y1 = std::max(y1, p.y);
      }
      // Keep the whole outline within reach of the picture.
      dx = std::clamp(dx, kPolyClampLo - x0, kPolyClampHi - x1);
      dy = std::clamp(dy, kPolyClampLo - y0, kPolyClampHi - y1);
      for (PolyPt &p : pts) {
        p.x += dx;
        p.y += dy;
      }
    });
    u.lastX += dx * W; // by what was really applied, so a clamped edge
    u.lastY += dy * H; // does not build up a debt
  } else if (d == MaskUi::Drag::Lasso) {
    const float gap = kLassoTolPx / state.zoom;
    if (!u.lassoRaw.empty() &&
        std::hypot(px - u.lassoRaw.back().x, py - u.lassoRaw.back().y) < gap) {
      state.markDirty();
      return;
    }
    if (u.lassoRaw.size() < kPolyMaxPoints * 8)
      u.lassoRaw.push_back(PolyPt{px, py});
    // Simplify the path so far; loosen the tolerance until it fits.
    float tol = kLassoTolPx / state.zoom;
    std::vector<PolyPt> simple = polySimplify(u.lassoRaw, tol);
    for (int guard = 0; simple.size() > kPolyMaxPoints && guard < 16; ++guard) {
      tol *= 2.0f;
      simple = polySimplify(u.lassoRaw, tol);
    }
    polygonEditPts(i, part, kMaskCtlPolyLasso, [&](std::vector<PolyPt> &pts) {
      pts.clear();
      for (const PolyPt &p : simple)
        pts.push_back(PolyPt{polyClampCoord(p.x / W), polyClampCoord(p.y / H)});
    });
  } else
    return;
  polygonDirty();
}

// Right-click on a vertex of the selected polygon removes it.
static void polygonRightPress(float lx, float ly) {
  if (!maskToolActive() || !selectedIsPolygon())
    return;
  const int i = selMask();
  if (i < 0)
    return;
  const size_t part = static_cast<size_t>(selPart());
  const MaskSet &ms = state.adjust.masks();
  if (part >= ms[static_cast<size_t>(i)].parts.size())
    return;
  const auto view = polygonInView(ms[static_cast<size_t>(i)].parts[part],
                                  state.adjust.geoFrame());
  if (!view)
    return;
  const float px = lx / state.zoom, py = ly / state.zoom;
  const int vi = polyVertexAt(polyDocPts(*view), px, py);
  if (vi < 0)
    return;
  const GeoFrame frame = state.adjust.geoFrame();
  maskEditDiscrete([&](Mask &m) {
    editShapeInView<PolygonShape>(m, part, frame, [&](PolygonShape &s) {
      if (static_cast<size_t>(vi) < s.pts.size())
        s.pts.erase(s.pts.begin() + vi);
    });
  });
}

