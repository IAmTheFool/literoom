// app/mask_brush.hpp: brush painting
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---- Brush painting ----
// With a brush part selected, every press on the image starts a stroke and the
// drag extends it; the whole stroke is one undo step (control kMaskCtlBrush,
// closed by maskEndDrag). Points are recorded in document pixels of the
// picture on screen and stored through the GeoFrame.

// A brush mask has no body to grab; its "pin" (where its first stroke began)
// is what an unselected brush is picked by.
static bool brushPinHit(const BrushShape &b, float px, float py) {
  if (b.strokes.empty() || b.strokes.front().pts.empty())
    return false;
  const BrushPt &p = b.strokes.front().pts.front();
  const float tol = kMaskHitPx / state.zoom;
  return std::hypot(px - p.x * state.docWidth(),
                    py - p.y * state.docHeight()) <= tol;
}

static void brushBegin(float px, float py) {
  const int i = selMask();
  if (i < 0)
    return;
  const size_t part = static_cast<size_t>(selPart());
  const float W = state.docWidth(), H = state.docHeight();
  const GeoFrame frame = state.adjust.geoFrame();
  const MaskUi &u = state.mask;
  BrushStroke st;
  st.erase = u.brushErase;
  st.size = BrushShape::sizeFromSource(u.brushSize, frame);
  st.feather = u.brushFeather;
  st.density = u.brushDensity;
  st.flow = u.brushFlow;
  st.pts.push_back(BrushShape::pointFromSource(BrushPt{px / W, py / H}, frame));
  bool full = false;
  maskEdit(
      i,
      [&](Mask &m) {
        auto *b = part < m.parts.size()
                      ? std::get_if<BrushShape>(&m.parts[part].shape)
                      : nullptr;
        if (!b)
          return;
        if (b->strokes.size() >= kBrushMaxStrokes) {
          full = true;
          return;
        }
        b->strokes.push_back(std::move(st));
      },
      kMaskCtlBrush);
  if (full) {
    setStatus("Brush stroke limit reached (" +
                  std::to_string(kBrushMaxStrokes) + ")",
              true);
    return;
  }
  state.mask.drag = MaskUi::Drag::Paint;
  state.mask.lastX = state.mask.curX = px;
  state.mask.lastY = state.mask.curY = py;
  enterDrag(false); // recompute on the cheap preview path while held
  state.markAdjustDirty();
  state.markDirty();
}

static void brushDragTo(float px, float py) {
  const int i = selMask();
  if (i < 0)
    return;
  MaskUi &u = state.mask;
  const size_t part = static_cast<size_t>(selPart());
  u.curX = px;
  u.curY = py;
  const float W = state.docWidth(), H = state.docHeight();
  const float rad = u.brushSize * std::min(W, H);
  // Skip points closer than a fraction of the radius: they add nothing to the
  // rendered stroke and would only bloat it (and every undo snapshot).
  const float gap = std::max(rad * kBrushPointGap, 0.5f / state.zoom);
  if (std::hypot(px - u.lastX, py - u.lastY) >= gap) {
    const GeoFrame frame = state.adjust.geoFrame();
    const BrushPt pt =
        BrushShape::pointFromSource(BrushPt{px / W, py / H}, frame);
    maskEdit(
        i,
        [&](Mask &m) {
          auto *b = part < m.parts.size()
                        ? std::get_if<BrushShape>(&m.parts[part].shape)
                        : nullptr;
          if (!b || b->strokes.empty())
            return;
          auto &pts = b->strokes.back().pts;
          if (pts.size() < kBrushMaxPoints)
            pts.push_back(pt);
        },
        kMaskCtlBrush);
    u.lastX = px;
    u.lastY = py;
    enterDrag(false);
    state.markAdjustDirty();
  }
  state.markDirty(); // the cursor ring follows the pointer
}

static std::shared_ptr<CanvasImage>
lumTintBase(); // defined with the luminosity overlay below

// Color of the picture at (fx, fy) (fractions of the picture on screen) as
// it stands just BEFORE mask `sel`: the same pixels ColorRangeEval sees.
static std::optional<std::array<float, 3>> samplePreMaskColor(int sel, float fx,
                                                              float fy) {
  const MaskSet &ms = state.adjust.masks();
  const auto base = lumTintBase();
  if (!base || sel < 0 || sel >= static_cast<int>(ms.size()))
    return std::nullopt;
  RenderParams rp = state.adjust.buildRenderParams();
  rp.film = {}; // the picture BEFORE the mask: no Vignette / Grain (they run
                // after the masks)
  rp.masks = buildMaskPasses(MaskSet(ms.begin(), ms.begin() + sel),
                             state.adjust.geoFrame());
  const auto pre = renderWith(*base, rp);
  const int x =
      std::clamp(static_cast<int>(fx * pre->width), 0, pre->width - 1);
  const int y =
      std::clamp(static_cast<int>(fy * pre->height), 0, pre->height - 1);
  // Average a 5x5 patch so one noisy pixel doesn't pick the color.
  float sum[3] = {0, 0, 0};
  int n = 0;
  for (int dy = -2; dy <= 2; ++dy)
    for (int dx = -2; dx <= 2; ++dx) {
      const int xx = std::clamp(x + dx, 0, pre->width - 1);
      const int yy = std::clamp(y + dy, 0, pre->height - 1);
      const uint8_t *p =
          &pre->pixels[(static_cast<size_t>(yy) * pre->width + xx) * 4];
      for (int c = 0; c < 3; ++c)
        sum[c] += p[c];
      ++n;
    }
  return std::array<float, 3>{sum[0] / (255.0f * n), sum[1] / (255.0f * n),
                              sum[2] / (255.0f * n)};
}

static void colorRangePress(float px, float py) {
  const float W = state.docWidth(), H = state.docHeight();
  if (W <= 0 || H <= 0)
    return;
  const auto c = samplePreMaskColor(selMask(), px / W, py / H);
  if (!c)
    return;
  const size_t part = static_cast<size_t>(selPart());
  maskEditDiscrete([&](Mask &m) {
    if (part < m.parts.size())
      if (auto *s = std::get_if<ColorRangeShape>(&m.parts[part].shape)) {
        s->set = true;
        s->r = (*c)[0];
        s->g = (*c)[1];
        s->b = (*c)[2];
      }
  });
}

static void maskPress(float lx, float ly) {
  if (!maskToolActive())
    return;
  state.mask.drag = MaskUi::Drag::None;
  const MaskSet &ms = state.adjust.masks();
  const GeoFrame frame = state.adjust.geoFrame();
  const float px = lx / state.zoom, py = ly / state.zoom;
  const int sel = selMask();
  const int selP = selPart();
  const int nMasks = static_cast<int>(ms.size());
  const auto partCount = [&](int i) {
    return static_cast<int>(ms[static_cast<size_t>(i)].parts.size());
  };
  const auto partAt = [&](int i, int j) -> const MaskPart & {
    return ms[static_cast<size_t>(i)].parts[static_cast<size_t>(j)];
  };
  // With a brush part selected every press paints (pick another part from
  // the list, or its pin if it is a brush).
  if (sel >= 0 && partCount(sel) > 0 &&
      std::holds_alternative<BrushShape>(partAt(sel, selP).shape)) {
    bool picked = false;
    for (int i = nMasks - 1; i >= 0 && !picked; --i)
      for (int j = partCount(i) - 1; j >= 0 && !picked; --j) {
        if (i == sel && j == selP)
          continue;
        if (const auto bv = brushInView(partAt(i, j), frame))
          if (brushPinHit(*bv, px, py)) {
            selectMask(i);
            state.mask.part = j;
            state.markDirty();
            picked = true;
          }
      }
    if (!picked)
      brushBegin(px, py);
    return;
  }

  if (sel >= 0 && partCount(sel) > 0 &&
      std::holds_alternative<ObjectShape>(partAt(sel, selP).shape)) {
    objectPress(lx, ly, false); // every press is a prompt, like a brush
    return;
  }
  // Polygon: every press edits the outline (see "Polygon / lasso tool").
  if (sel >= 0 && partCount(sel) > 0 &&
      std::holds_alternative<PolygonShape>(partAt(sel, selP).shape)) {
    polygonBegin(px, py);
    return;
  }
  // Color Range: every press samples a new color (like a brush, no handles).
  if (sel >= 0 && partCount(sel) > 0 &&
      std::holds_alternative<ColorRangeShape>(partAt(sel, selP).shape)) {
    colorRangePress(px, py);
    return;
  }
  // Tries part `j` of mask `i`; on a hit selects it and starts the matching
  // drag.
  auto tryPart = [&](int i, int j, bool isSelected) -> bool {
    const MaskPart &m = partAt(i, j);
    int hit = kMaskHitNone;
    float shapeCx = 0, shapeCy = 0; // center, fraction of the picture
    if (const auto rv = radialInView(m, frame)) {
      hit = maskHit(*rv, px, py, isSelected);
      shapeCx = rv->cx;
      shapeCy = rv->cy;
    } else if (const auto lv = linearInView(m, frame)) {
      hit = linearHit(*lv, px, py, isSelected);
      shapeCx = lv->cx;
      shapeCy = lv->cy;
    } else if (const auto bv = brushInView(m, frame)) {
      // Nothing to grab on a brush: its pin only selects it.
      if (!brushPinHit(*bv, px, py))
        return false;
      selectMask(i);
      state.mask.part = j;
      state.markDirty();
      return true;
    } else if (const auto qv = rectInView(m, frame)) {
      hit = rectHit(*qv, px, py, isSelected);
      shapeCx = qv->cx;
      shapeCy = qv->cy;
    } else if (const auto pv = polygonInView(m, frame)) {
      // An unselected polygon is only picked (its vertices are edited once
      // it is selected).
      if (!polygonBodyHit(*pv, px, py))
        return false;
      selectMask(i);
      state.mask.part = j;
      state.markDirty();
      return true;
    }
    if (hit == kMaskHitNone)
      return false;
    selectMask(i);
    state.mask.part = j;
    if (hit == kMaskHitX) {
      state.mask.drag = MaskUi::Drag::ResizeX;
    } else if (hit == kMaskHitY) {
      state.mask.drag = MaskUi::Drag::ResizeY;
    } else if (hit == kMaskHitLinEnd) {
      state.mask.drag = MaskUi::Drag::LinEnd;
    } else if (hit == kMaskHitLinStart) {
      state.mask.drag = MaskUi::Drag::LinStart;
    } else {
      state.mask.drag = MaskUi::Drag::Move;
      state.mask.grabX = px - shapeCx * state.docWidth();
      state.mask.grabY = py - shapeCy * state.docHeight();
    }
    state.markDirty();
    return true;
  };
  if (sel >= 0 && partCount(sel) > 0 && tryPart(sel, selP, true))
    return;
  // The selected mask's other parts next (last part first), so a click on one
  // of them picks that part rather than a mask underneath.
  if (sel >= 0)
    for (int j = partCount(sel) - 1; j >= 0; --j)
      if (j != selP && tryPart(sel, j, false))
        return;
  // Otherwise clicking inside another mask selects it (topmost first).
  for (int i = nMasks - 1; i >= 0; --i) {
    if (i == sel)
      continue;
    for (int j = partCount(i) - 1; j >= 0; --j)
      if (tryPart(i, j, false))
        return;
  }
}

static void maskDrag(float lx, float ly) {
  if (!maskToolActive() || state.mask.drag == MaskUi::Drag::None)
    return;
  const int i = selMask();
  if (i < 0)
    return;
  const float W = state.docWidth(), H = state.docHeight();
  const float px = lx / state.zoom, py = ly / state.zoom;
  const MaskUi::Drag d = state.mask.drag;
  if (d == MaskUi::Drag::Paint) {
    brushDragTo(px, py);
    return;
  }
  if (d == MaskUi::Drag::PolyVertex || d == MaskUi::Drag::PolyMove ||
      d == MaskUi::Drag::Lasso) {
    polygonDragTo(px, py, d);
    return;
  }
  const float grabX = state.mask.grabX, grabY = state.mask.grabY;
  const GeoFrame frame = state.adjust.geoFrame();
  const size_t part = static_cast<size_t>(selPart());
  maskEdit(
      i,
      [&](Mask &m) {
        editRadialInView(m, part, frame, [&](RadialShape &r) {
          if (d == MaskUi::Drag::Move) {
            r.cx = std::clamp((px - grabX) / W, 0.0f, 1.0f);
            r.cy = std::clamp((py - grabY) / H, 0.0f, 1.0f);
          } else {
            // Distance of the pointer from the center along the rotated axis.
            const MaskGeom g = radialGeom(r);
            const float dx = px - g.cx, dy = py - g.cy;
            if (d == MaskUi::Drag::ResizeX)
              r.rx = std::clamp(std::fabs(dx * g.c + dy * g.s) / g.S,
                                kMaskMinRadius, kMaskMaxRadius);
            else
              r.ry = std::clamp(std::fabs(-dx * g.s + dy * g.c) / g.S,
                                kMaskMinRadius, kMaskMaxRadius);
          }
        });
        editShapeInView<RectShape>(m, part, frame, [&](RectShape &r) {
          if (d == MaskUi::Drag::Move) {
            r.cx = std::clamp((px - grabX) / W, 0.0f, 1.0f);
            r.cy = std::clamp((py - grabY) / H, 0.0f, 1.0f);
          } else {
            // Distance of the pointer from the center along the rotated axis.
            const MaskGeom g = rectGeom(r);
            const float dx = px - g.cx, dy = py - g.cy;
            if (d == MaskUi::Drag::ResizeX)
              r.rx = std::clamp(std::fabs(dx * g.c + dy * g.s) / g.S,
                                kMaskMinRadius, kMaskMaxRadius);
            else if (d == MaskUi::Drag::ResizeY)
              r.ry = std::clamp(std::fabs(-dx * g.s + dy * g.c) / g.S,
                                kMaskMinRadius, kMaskMaxRadius);
          }
        });
        editLinearInView(m, part, frame, [&](LinearShape &l) {
          if (d == MaskUi::Drag::Move) {
            l.cx = std::clamp((px - grabX) / W, 0.0f, 1.0f);
            l.cy = std::clamp((py - grabY) / H, 0.0f, 1.0f);
          } else {
            // Dragging a line's handle sets the half length AND turns the
            // axis so the handle stays under the pointer (the start handle
            // is on the opposite side of the center, so its pull is negated).
            const LinGeom g = linearGeom(l);
            const float sign = d == MaskUi::Drag::LinEnd ? 1.0f : -1.0f;
            const float dx = (px - g.cx) * sign, dy = (py - g.cy) * sign;
            const float dist = std::hypot(dx, dy);
            l.len = std::clamp(dist / g.S, kMaskMinRadius, kMaskMaxRadius);
            if (dist > 1e-3f)
              l.angle = std::atan2(dy, dx) * (180.0f / 3.14159265358979f);
          }
        });
      },
      d == MaskUi::Drag::Move      ? kMaskCtlMove
      : d == MaskUi::Drag::ResizeX ? kMaskCtlResizeX
      : d == MaskUi::Drag::ResizeY ? kMaskCtlResizeY
      : d == MaskUi::Drag::LinEnd  ? kMaskCtlLinEnd
                                   : kMaskCtlLinStart);
  enterDrag(false); // recompute on the cheap preview path while held
  state.markAdjustDirty();
  state.markDirty(); // the overlay follows the pointer
}

// Red tint showing the mask's weight (1 = solid, 0 = clear) over the picture.
static void paintRadialTint(CanvasContext &ctx, const RadialShape &r,
                            bool invert) {
  const MaskGeom g = radialGeom(r);
  const float W = state.docWidth(), H = state.docHeight();
  const float inner = r.innerFrac();
  const Color on = maskTintColor(120), off = maskTintColor(0);
  ctx.save();
  ctx.beginPath();
  ctx.rect(0, 0, W, H);
  ctx.clip();
  // Unit-circle gradient under the ellipse's own transform.
  ctx.translate(g.cx, g.cy);
  ctx.rotate(g.ang);
  ctx.scale(g.rx, g.ry);
  CanvasGradient grad = CanvasGradient::radial(0, 0, 0, 0, 0, 1);
  grad.addColorStop(0.0f, invert ? off : on);
  grad.addColorStop(inner, invert ? off : on);
  grad.addColorStop(1.0f, invert ? on : off);
  ctx.setFillGradient(grad);
  const float R = (W + H) / std::min(g.rx, g.ry) + 2.0f; // covers the document
  ctx.fillRect(-R, -R, 2.0f * R, 2.0f * R);
  ctx.restore();
}

// Outline of one radial mask; the selected one also gets its feather ring
// and handles.
static void paintRadialOutline(CanvasContext &ctx, const RadialShape &r,
                               bool selected) {
  const MaskGeom g = radialGeom(r);
  const float px = 1.0f / state.zoom; // one screen pixel, in image units
  constexpr float kTwoPi = 6.28318530717959f;

  ctx.beginPath();
  ctx.ellipse(g.cx, g.cy, g.rx, g.ry, g.ang, 0.0f, kTwoPi);
  ctx.setStrokeColor(Color{0, 0, 0, selected ? uint8_t{150} : uint8_t{90}});
  ctx.setLineWidth(px * 3.0f);
  ctx.stroke();
  ctx.setStrokeColor(
      Color{255, 255, 255, selected ? uint8_t{255} : uint8_t{140}});
  ctx.setLineWidth(px * (selected ? 1.5f : 1.0f));
  ctx.stroke();
  if (!selected)
    return;

  // Where the feather begins (skipped when it would sit on the outline or
  // collapse into the center).
  const float inner = r.innerFrac();
  if (r.feather > 0.02f && inner > 0.02f) {
    ctx.beginPath();
    ctx.ellipse(g.cx, g.cy, g.rx * inner, g.ry * inner, g.ang, 0.0f, kTwoPi);
    ctx.setLineDash({4.0f * px, 4.0f * px});
    ctx.setStrokeColor(Color{255, 255, 255, 170});
    ctx.setLineWidth(px);
    ctx.stroke();
    ctx.setLineDash({});
  }

  // Four square handles on the axes, and a ring at the center.
  const float hs = kMaskHandlePx * px;
  const float hx[4] = {g.cx + g.rx * g.c, g.cx - g.rx * g.c, g.cx - g.ry * g.s,
                       g.cx + g.ry * g.s};
  const float hy[4] = {g.cy + g.rx * g.s, g.cy - g.rx * g.s, g.cy + g.ry * g.c,
                       g.cy - g.ry * g.c};
  ctx.setFillColor(kColorWhite);
  ctx.setStrokeColor(Color{60, 60, 60, 255});
  ctx.setLineWidth(px);
  for (int k = 0; k < 4; ++k) {
    ctx.fillRect(hx[k] - hs / 2.0f, hy[k] - hs / 2.0f, hs, hs);
    ctx.strokeRect(hx[k] - hs / 2.0f, hy[k] - hs / 2.0f, hs, hs);
  }
  ctx.beginPath();
  ctx.arc(g.cx, g.cy, hs * 0.55f, 0.0f, kTwoPi);
  ctx.setFillColor(Color{255, 255, 255, 60});
  ctx.fill();
  ctx.setStrokeColor(kColorWhite);
  ctx.setLineWidth(px * 1.5f);
  ctx.stroke();
}

// Outline of one rectangle mask; the selected one also gets its feather
// rectangle, handles (middle of each side) and a ring at the center.
static void paintRectOutline(CanvasContext &ctx, const RectShape &r,
                             bool selected) {
  const MaskGeom g = rectGeom(r);
  const float px = 1.0f / state.zoom; // one screen pixel, in image units
  constexpr float kTwoPi = 6.28318530717959f;
  // The rectangle `k` times the half-sizes, as a closed path.
  const auto path = [&](float k) {
    const float ax = g.rx * k * g.c, ay = g.rx * k * g.s;  // along rx
    const float bx = -g.ry * k * g.s, by = g.ry * k * g.c; // along ry
    ctx.beginPath();
    ctx.moveTo(g.cx - ax - bx, g.cy - ay - by);
    ctx.lineTo(g.cx + ax - bx, g.cy + ay - by);
    ctx.lineTo(g.cx + ax + bx, g.cy + ay + by);
    ctx.lineTo(g.cx - ax + bx, g.cy - ay + by);
    ctx.closePath();
  };
  path(1.0f);
  ctx.setStrokeColor(Color{0, 0, 0, selected ? uint8_t{150} : uint8_t{90}});
  ctx.setLineWidth(px * 3.0f);
  ctx.stroke();
  ctx.setStrokeColor(
      Color{255, 255, 255, selected ? uint8_t{255} : uint8_t{140}});
  ctx.setLineWidth(px * (selected ? 1.5f : 1.0f));
  ctx.stroke();
  if (!selected)
    return;

  // Where the feather begins (skipped when it sits on the outline or
  // collapses).
  const float inner = r.innerFrac();
  if (r.feather > 0.02f && inner > 0.02f) {
    path(inner);
    ctx.setLineDash({4.0f * px, 4.0f * px});
    ctx.setStrokeColor(Color{255, 255, 255, 170});
    ctx.setLineWidth(px);
    ctx.stroke();
    ctx.setLineDash({});
  }

  // Four square handles on the axes, and a ring at the center.
  const float hs = kMaskHandlePx * px;
  const float hx[4] = {g.cx + g.rx * g.c, g.cx - g.rx * g.c, g.cx - g.ry * g.s,
                       g.cx + g.ry * g.s};
  const float hy[4] = {g.cy + g.rx * g.s, g.cy - g.rx * g.s, g.cy + g.ry * g.c,
                       g.cy - g.ry * g.c};
  ctx.setFillColor(kColorWhite);
  ctx.setStrokeColor(Color{60, 60, 60, 255});
  ctx.setLineWidth(px);
  for (int k = 0; k < 4; ++k) {
    ctx.fillRect(hx[k] - hs / 2.0f, hy[k] - hs / 2.0f, hs, hs);
    ctx.strokeRect(hx[k] - hs / 2.0f, hy[k] - hs / 2.0f, hs, hs);
  }
  ctx.beginPath();
  ctx.arc(g.cx, g.cy, hs * 0.55f, 0.0f, kTwoPi);
  ctx.setFillColor(Color{255, 255, 255, 60});
  ctx.fill();
  ctx.setStrokeColor(kColorWhite);
  ctx.setLineWidth(px * 1.5f);
  ctx.stroke();
}

// Outline of one polygon mask. The outline is closed once it has three
// points; the selected one also shows a square handle on every vertex (the
// first one filled, so the start of a lasso is easy to see).
static void paintPolygonOutline(CanvasContext &ctx, const PolygonShape &view,
                                bool selected) {
  if (view.pts.empty())
    return;
  const std::vector<PolyPt> dp = polyDocPts(view);
  const float px = 1.0f / state.zoom; // one screen pixel, in image units
  const auto path = [&] {
    ctx.beginPath();
    ctx.moveTo(dp[0].x, dp[0].y);
    for (size_t i = 1; i < dp.size(); ++i)
      ctx.lineTo(dp[i].x, dp[i].y);
    if (dp.size() >= 3)
      ctx.closePath();
  };
  if (dp.size() >= 2) {
    path();
    ctx.setStrokeColor(Color{0, 0, 0, selected ? uint8_t{150} : uint8_t{90}});
    ctx.setLineWidth(px * 3.0f);
    ctx.stroke();
    ctx.setStrokeColor(
        Color{255, 255, 255, selected ? uint8_t{255} : uint8_t{140}});
    ctx.setLineWidth(px * (selected ? 1.5f : 1.0f));
    ctx.stroke();
  }
  if (!selected)
    return;
  const float hs = kMaskHandlePx * px * 0.8f;
  ctx.setStrokeColor(Color{60, 60, 60, 255});
  ctx.setLineWidth(px);
  for (size_t i = 0; i < dp.size(); ++i) {
    ctx.setFillColor(i == 0 ? Color{255, 210, 60, 255} : kColorWhite);
    ctx.fillRect(dp[i].x - hs / 2.0f, dp[i].y - hs / 2.0f, hs, hs);
    ctx.strokeRect(dp[i].x - hs / 2.0f, dp[i].y - hs / 2.0f, hs, hs);
  }
}

// Red tint of a linear mask: a gradient along the axis from the weight-1 line
// to the weight-0 line, with stops sampled from the same smoothstep the
// renderer uses (the gradient itself is piecewise linear).
static void paintLinearTint(CanvasContext &ctx, const LinearShape &l,
                            bool invert) {
  const LinGeom g = linearGeom(l);
  const float W = state.docWidth(), H = state.docHeight();
  const auto alphaAt = [invert](float t) -> Color {
    float w = 1.0f - t * t * (3.0f - 2.0f * t);
    if (invert)
      w = 1.0f - w;
    return maskTintColor(static_cast<uint8_t>(std::lround(120.0f * w)));
  };
  CanvasGradient grad = CanvasGradient::linear(g.sx(), g.sy(), g.ex(), g.ey());
  constexpr int kStops = 8;
  for (int k = 0; k <= kStops; ++k) {
    const float t = static_cast<float>(k) / kStops;
    grad.addColorStop(t, alphaAt(t));
  }
  ctx.save();
  ctx.setFillGradient(grad);
  ctx.fillRect(0, 0, W, H); // the gradient pads beyond both lines
  ctx.restore();
}

// Outline of one linear mask: the two lines of the ramp; the selected one
// also gets the center line, the axis with an arrowhead, and its handles.
static void paintLinearOutline(CanvasContext &ctx, const LinearShape &l,
                               bool selected) {
  const LinGeom g = linearGeom(l);
  const float px = 1.0f / state.zoom; // one screen pixel, in image units
  constexpr float kTwoPi = 6.28318530717959f;
  const float W = state.docWidth(), H = state.docHeight();
  const float R = W + H;             // long enough to cross the document
  const float vx = -g.uy, vy = g.ux; // along the lines
  const auto line = [&](float ax, float ay) {
    ctx.beginPath();
    ctx.moveTo(ax - vx * R, ay - vy * R);
    ctx.lineTo(ax + vx * R, ay + vy * R);
  };
  const auto strokeTwice = [&](float wOuter, float wInner, uint8_t a1,
                               uint8_t a2) {
    ctx.setStrokeColor(Color{0, 0, 0, a1});
    ctx.setLineWidth(px * wOuter);
    ctx.stroke();
    ctx.setStrokeColor(Color{255, 255, 255, a2});
    ctx.setLineWidth(px * wInner);
    ctx.stroke();
  };

  // The lines are clipped to the picture; handles are drawn outside the clip.
  ctx.save();
  ctx.beginPath();
  ctx.rect(0, 0, W, H);
  ctx.clip();
  const uint8_t a1 = selected ? uint8_t{150} : uint8_t{90};
  const uint8_t a2 = selected ? uint8_t{255} : uint8_t{140};
  line(g.sx(), g.sy());
  strokeTwice(3.0f, selected ? 1.5f : 1.0f, a1, a2);
  line(g.ex(), g.ey());
  strokeTwice(3.0f, selected ? 1.5f : 1.0f, a1, a2);
  if (selected) {
    line(g.cx, g.cy); // the 50% line
    ctx.setLineDash({4.0f * px, 4.0f * px});
    ctx.setStrokeColor(Color{255, 255, 255, 170});
    ctx.setLineWidth(px);
    ctx.stroke();
    ctx.setLineDash({});
  }
  ctx.restore();
  if (!selected)
    return;

  // The axis between the handles, with an arrowhead showing the fade-out.
  const float hs = kMaskHandlePx * px;
  ctx.beginPath();
  ctx.moveTo(g.sx(), g.sy());
  ctx.lineTo(g.ex(), g.ey());
  ctx.setStrokeColor(Color{255, 255, 255, 200});
  ctx.setLineWidth(px);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(g.ex() + g.ux * hs * 1.6f, g.ey() + g.uy * hs * 1.6f);
  ctx.lineTo(g.ex() + g.ux * hs * 0.7f + vx * hs * 0.7f,
             g.ey() + g.uy * hs * 0.7f + vy * hs * 0.7f);
  ctx.lineTo(g.ex() + g.ux * hs * 0.7f - vx * hs * 0.7f,
             g.ey() + g.uy * hs * 0.7f - vy * hs * 0.7f);
  ctx.closePath();
  ctx.setFillColor(kColorWhite);
  ctx.fill();

  // Square handles on the two lines, and a ring at the center.
  const float hx[2] = {g.sx(), g.ex()}, hy[2] = {g.sy(), g.ey()};
  ctx.setFillColor(kColorWhite);
  ctx.setStrokeColor(Color{60, 60, 60, 255});
  ctx.setLineWidth(px);
  for (int k = 0; k < 2; ++k) {
    ctx.fillRect(hx[k] - hs / 2.0f, hy[k] - hs / 2.0f, hs, hs);
    ctx.strokeRect(hx[k] - hs / 2.0f, hy[k] - hs / 2.0f, hs, hs);
  }
  ctx.beginPath();
  ctx.arc(g.cx, g.cy, hs * 0.55f, 0.0f, kTwoPi);
  ctx.setFillColor(Color{255, 255, 255, 60});
  ctx.fill();
  ctx.setStrokeColor(kColorWhite);
  ctx.setLineWidth(px * 1.5f);
  ctx.stroke();
}

