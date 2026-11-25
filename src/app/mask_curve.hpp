// app/mask_curve.hpp: mask tone curve editor (canvas side)
#pragma once

#include <algorithm> // std::clamp
#include <cstddef>   // size_t
#include <cstdint>   // uint8_t, uint64_t

#include "liteui.hpp"             // View, Size, Cursor, CanvasContext, Color

#include "app/constants.hpp"      // kCurvePadPx, kCurveHitPx, kCurvePointPx,
                                  // kColorWhite, kColorDividerHover
#include "app/curve_editor.hpp"   // curveChannelColor
#include "app/editor_state.hpp"   // inline EditorState state
#include "app/mask_effects.hpp"   // kMaskCurveSizePx, kMaskCtlCurve
#include "app/masks.hpp"          // selMask, selectedMask, maskEdit
#include "app/render_update.hpp"  // enterDrag
#include "app/ui_state.hpp"       // uiState
#include "app/widgets.hpp"        // bindDrag
#include "color/curves.hpp"       // CurvePoint, CurvePoints, CurveChannel, kCurveChannelCount,
                                  // kCurveMaxPoints, curveInsert, curveMovePoint,
                                  // curveRemove, curveToLut, identityCurve
#include "mask/mask.hpp"          // Mask (.curves)

// ---- Mask tone curve editor (canvas side) ----
// Same behavior as the Curve pane (click = add point, drag = move,
// right-click = delete) but on the selected mask's own curves, and a bit
// larger (kMaskCurveSizePx).
inline float mcPlotPx() { return kMaskCurveSizePx - 2.0f * kCurvePadPx; }
inline CurvePoint mcFromLocal(float lx, float ly) {
  const float plot = mcPlotPx();
  return {
      std::clamp((lx - kCurvePadPx) / plot * 255.0f, 0.0f, 255.0f),
      std::clamp(255.0f - (ly - kCurvePadPx) / plot * 255.0f, 0.0f, 255.0f)};
}
inline void mcToLocal(float x, float y, float &lx, float &ly) {
  const float plot = mcPlotPx();
  lx = kCurvePadPx + x / 255.0f * plot;
  ly = kCurvePadPx + (255.0f - y) / 255.0f * plot;
}
inline int mcHit(const CurvePoints &pts, float lx, float ly) {
  int hit = -1;
  float best = kCurveHitPx * kCurveHitPx;
  for (size_t i = 0; i < pts.size(); ++i) {
    float px, py;
    mcToLocal(pts[i].x, pts[i].y, px, py);
    const float d2 = (px - lx) * (px - lx) + (py - ly) * (py - ly);
    if (d2 <= best) {
      best = d2;
      hit = static_cast<int>(i);
    }
  }
  return hit;
}

// The curve being edited, copied (empty list if no mask is selected).
inline CurvePoints maskCurveCopy() {
  const Mask *m = selectedMask();
  return m ? m->curves[static_cast<size_t>(uiState.maskPane.curveCh)] : CurvePoints{};
}
// coalesce = true folds a drag into one undo step; false = its own step.
inline void maskCurveCommit(const CurvePoints &pts, bool coalesce) {
  const CurveChannel ch = uiState.maskPane.curveCh;
  maskEdit(
      selMask(), [&](Mask &m) { m.curves[static_cast<size_t>(ch)] = pts; },
      coalesce ? kMaskCtlCurve + static_cast<int>(ch) : -1);
}

inline void maskCurvePress(float lx, float ly) {
  if (!state.original || !selectedMask())
    return;
  CurvePoints pts = maskCurveCopy();
  int hit = mcHit(pts, lx, ly);
  uiState.maskPane.curveGrabDx = uiState.maskPane.curveGrabDy = 0.0f;
  if (hit >= 0) {
    const CurvePoint m = mcFromLocal(lx, ly);
    uiState.maskPane.curveGrabDx = pts[static_cast<size_t>(hit)].x - m.x;
    uiState.maskPane.curveGrabDy = pts[static_cast<size_t>(hit)].y - m.y;
  } else {
    if (pts.size() >= kCurveMaxPoints)
      return;
    const CurvePoint m = mcFromLocal(lx, ly);
    hit = curveInsert(pts, m.x, m.y);
    if (hit < 0)
      return;
    maskCurveCommit(pts, true); // the drag that follows folds into this step
    state.markAdjustDirty();
  }
  uiState.maskPane.curveActive = hit;
  uiState.maskPane.curveUiDirty = true;
}

inline void maskCurveDrag(float lx, float ly) {
  if (uiState.maskPane.curveActive < 0 || !state.original || !selectedMask())
    return;
  CurvePoints pts = maskCurveCopy();
  if (uiState.maskPane.curveActive >= static_cast<int>(pts.size()))
    return;
  const CurvePoint m = mcFromLocal(lx, ly);
  const CurvePoints before = pts;
  curveMovePoint(pts, uiState.maskPane.curveActive, m.x + uiState.maskPane.curveGrabDx,
                 m.y + uiState.maskPane.curveGrabDy);
  if (pts == before)
    return;
  maskCurveCommit(pts, true);
  enterDrag(false); // switches recompute+paint to the preview path
  state.markAdjustDirty();
}

inline void maskCurveEnd() {
  uiState.maskPane.curveActive = -1;
  uiState.maskPane.curveUiDirty = true;
  state.adjust.endMaskEdit();
  if (state.dragging) { // leave preview mode: one full-resolution recompute
    state.dragging = false;
    state.dragIsAngle = false;
    state.markAdjustDirty();
  }
}

inline void maskCurveRightPress(float lx, float ly) {
  if (!state.original || !selectedMask())
    return;
  CurvePoints pts = maskCurveCopy();
  if (curveRemove(pts, mcHit(pts, lx, ly))) {
    maskCurveCommit(pts, false);
    state.markAdjustDirty();
  }
}

inline void paintMaskCurve(CanvasContext &ctx) {
  const float W = ctx.width(), H = ctx.height();
  const float pad = kCurvePadPx, plot = mcPlotPx();
  ctx.setFillColor(Color{34, 34, 38, 255});
  ctx.fillRect(0, 0, W, H);

  ctx.setLineWidth(1.0f);
  ctx.setStrokeColor(Color{255, 255, 255, 30});
  ctx.beginPath();
  for (int q = 0; q <= 4; ++q) {
    const float t = pad + plot * static_cast<float>(q) / 4.0f;
    ctx.moveTo(t, pad);
    ctx.lineTo(t, pad + plot);
    ctx.moveTo(pad, t);
    ctx.lineTo(pad + plot, t);
  }
  ctx.stroke();
  ctx.setStrokeColor(Color{255, 255, 255, 70});
  ctx.beginPath();
  ctx.moveTo(pad, pad + plot);
  ctx.lineTo(pad + plot, pad);
  ctx.stroke();

  const Mask *m = selectedMask();
  if (!m)
    return;
  auto strokeCurve = [&](CurveChannel ch, uint8_t alpha, float width) {
    const auto lut = curveToLut(m->curves[static_cast<size_t>(ch)]);
    ctx.setStrokeColor(curveChannelColor(ch, alpha));
    ctx.setLineWidth(width);
    ctx.beginPath();
    for (int i = 0; i < 256; ++i) {
      float lx, ly;
      mcToLocal(static_cast<float>(i), lut[static_cast<size_t>(i)], lx, ly);
      if (i == 0)
        ctx.moveTo(lx, ly);
      else
        ctx.lineTo(lx, ly);
    }
    ctx.stroke();
  };
  const CurvePoints straight = identityCurve();
  for (int c = 0; c < static_cast<int>(kCurveChannelCount); ++c) {
    const auto ch = static_cast<CurveChannel>(c);
    if (ch != uiState.maskPane.curveCh && !(m->curves[static_cast<size_t>(c)] == straight))
      strokeCurve(ch, 110, 1.0f);
  }
  strokeCurve(uiState.maskPane.curveCh, 255, 1.75f);

  const CurvePoints &pts = m->curves[static_cast<size_t>(uiState.maskPane.curveCh)];
  ctx.setLineWidth(1.0f);
  for (size_t i = 0; i < pts.size(); ++i) {
    float lx, ly;
    mcToLocal(pts[i].x, pts[i].y, lx, ly);
    const bool active = static_cast<int>(i) == uiState.maskPane.curveActive;
    ctx.setFillColor(active ? kColorDividerHover : kColorWhite);
    ctx.setStrokeColor(Color{20, 20, 20, 255});
    ctx.fillRect(lx - kCurvePointPx / 2.0f, ly - kCurvePointPx / 2.0f,
                 kCurvePointPx, kCurvePointPx);
    ctx.strokeRect(lx - kCurvePointPx / 2.0f, ly - kCurvePointPx / 2.0f,
                   kCurvePointPx, kCurvePointPx);
  }
}

inline View buildMaskCurveCanvas() {
  View c;
  c.isCanvas = true;
  c.style.width = Size::pixel(kMaskCurveSizePx);
  c.style.height = Size::pixel(kMaskCurveSizePx);
  c.style.flexShrink = 0;
  c.onPaint = [](CanvasContext &ctx) { paintMaskCurve(ctx); };
  c.canvasDirtySource = [] {
    // Repaint on any edit (the version moves with every change, undo and
    // redo too), a different mask / tab, or a UI-only change (channel,
    // selected point).
    static uint64_t lastVer = ~0ull;
    static int lastSel = -2, lastTabs = -2;
    const uint64_t ver = state.adjust.version();
    const int sel = selMask();
    const int tabs = state.maskTab * 10 + state.maskFx;
    const bool ui = uiState.maskPane.curveUiDirty;
    uiState.maskPane.curveUiDirty = false;
    if (ver == lastVer && sel == lastSel && tabs == lastTabs && !ui)
      return false;
    lastVer = ver;
    lastSel = sel;
    lastTabs = tabs;
    return true;
  };
  bindDrag(
      c, [](float lx, float ly) { maskCurvePress(lx, ly); },
      [](float lx, float ly) { maskCurveDrag(lx, ly); }, [] { maskCurveEnd(); },
      [](float lx, float ly) { maskCurveRightPress(lx, ly); },
      Cursor::Crosshair);
  return c;
}

