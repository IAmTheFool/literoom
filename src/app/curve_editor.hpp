// app/curve_editor.hpp: tone curve editor (canvas side)

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "liteui.hpp"             // CanvasContext, Color
#include "app/constants.hpp"      // kCurveSizePx/PadPx/HitPx/PointPx, kColorWhite, kColorDividerHover
#include "app/editor_state.hpp"   // state (adjust, original, dragging, hist*)
#include "app/render_update.hpp"  // enterDrag()
#include "app/ui_state.hpp"       // uiState (curve.*)
#include "color/curves.hpp"       // CurvePoint(s), CurveChannel, kCurveMaxPoints, kCurveChannelCount,
                                  // curveInsert/MovePoint/Remove, identityCurve, curveToLut

// ---------------- Tone curve editor (canvas side) ----------------
// Mapping between canvas-local screen px and curve units (0..255, y up).
inline float curvePlotPx() { return kCurveSizePx - 2.0f * kCurvePadPx; }

inline CurvePoint curveFromLocal(float lx, float ly) {
  const float plot = curvePlotPx();
  return {
      std::clamp((lx - kCurvePadPx) / plot * 255.0f, 0.0f, 255.0f),
      std::clamp(255.0f - (ly - kCurvePadPx) / plot * 255.0f, 0.0f, 255.0f)};
}

inline void curveToLocal(float x, float y, float &lx, float &ly) {
  const float plot = curvePlotPx();
  lx = kCurvePadPx + x / 255.0f * plot;
  ly = kCurvePadPx + (255.0f - y) / 255.0f * plot;
}

// Index of the point within kCurveHitPx of (lx, ly), nearest wins; -1 if none.
inline int curveHit(const CurvePoints &pts, float lx, float ly) {
  int hit = -1;
  float best = kCurveHitPx * kCurveHitPx;
  for (size_t i = 0; i < pts.size(); ++i) {
    float px, py;
    curveToLocal(pts[i].x, pts[i].y, px, py);
    const float d2 = (px - lx) * (px - lx) + (py - ly) * (py - ly);
    if (d2 <= best) {
      best = d2;
      hit = static_cast<int>(i);
    }
  }
  return hit;
}

inline const CurvePoints &curveActiveCurve() {
  return state.adjust.curve(uiState.curve.channel);
}
inline void curvePress(float lx, float ly) {
  if (!state.original)
    return;
  CurvePoints pts = curveActiveCurve();
  int hit = curveHit(pts, lx, ly);
  uiState.curve.grabDx = uiState.curve.grabDy = 0.0f;
  if (hit >= 0) {
    const CurvePoint m = curveFromLocal(lx, ly);
    uiState.curve.grabDx = pts[hit].x - m.x;
    uiState.curve.grabDy = pts[hit].y - m.y;
  } else {
    if (pts.size() >= kCurveMaxPoints)
      return;
    const CurvePoint m = curveFromLocal(lx, ly);
    hit = curveInsert(pts, m.x, m.y);
    if (hit < 0)
      return;
    // coalesce = true: the drag that follows folds into this same undo step.
    state.adjust.setCurve(uiState.curve.channel, pts, true);
    // One full-resolution recompute for the click itself; the preview path
    // only starts once the point actually moves (see curveDrag), so a plain
    // click can never leave the view stuck on the low-res preview.
    state.markAdjustDirty();
  }
  uiState.curve.active = hit;
  uiState.curve.uiDirty = true; // show the selected point
}

inline void curveDrag(float lx, float ly) {
  if (uiState.curve.active < 0 || !state.original)
    return;
  CurvePoints pts = curveActiveCurve();
  if (uiState.curve.active >= static_cast<int>(pts.size()))
    return;
  const CurvePoint m = curveFromLocal(lx, ly);
  curveMovePoint(pts, uiState.curve.active, m.x + uiState.curve.grabDx, m.y + uiState.curve.grabDy);
  if (pts == curveActiveCurve())
    return;
  state.adjust.setCurve(uiState.curve.channel, pts, true);
  enterDrag(false); // switches recompute+paint to the preview path
  state.markAdjustDirty();
}

inline void curveEnd() {
  uiState.curve.active = -1;
  uiState.curve.uiDirty = true;
  state.adjust.endCurveEdit();
  if (state.dragging) { // leave preview mode: one full-resolution recompute
    state.dragging = false;
    state.dragIsAngle = false;
    state.markAdjustDirty();
  }
}

inline void curveRightPress(float lx, float ly) {
  if (!state.original)
    return;
  CurvePoints pts = curveActiveCurve();
  if (curveRemove(pts, curveHit(pts, lx, ly))) {
    state.adjust.setCurve(uiState.curve.channel, pts, false);
    state.markAdjustDirty();
  }
}

inline void setCurveChannel(CurveChannel ch) {
  uiState.curve.active = -1;
  uiState.curve.channel = ch;
  uiState.curve.uiDirty = true;
}

// Resets the curve being edited (one undo step).
inline void resetCurve() {
  uiState.curve.active = -1;
  state.adjust.setCurve(uiState.curve.channel, identityCurve(), false);
  uiState.curve.uiDirty = true;
  state.markAdjustDirty();
}

// Resets all four curves (one undo step).
inline void resetAllCurves() {
  uiState.curve.active = -1;
  state.adjust.resetAllCurves();
  uiState.curve.uiDirty = true;
  state.markAdjustDirty();
}

inline const char *curveChannelName(CurveChannel ch) {
  switch (ch) {
  case CurveChannel::Red:
    return "Red";
  case CurveChannel::Green:
    return "Green";
  case CurveChannel::Blue:
    return "Blue";
  default:
    return "RGB";
  }
}

// Line/tint color for a curve: white for the master, channel color otherwise.
inline Color curveChannelColor(CurveChannel ch, uint8_t alpha) {
  switch (ch) {
  case CurveChannel::Red:
    return Color{235, 70, 70, alpha};
  case CurveChannel::Green:
    return Color{70, 200, 90, alpha};
  case CurveChannel::Blue:
    return Color{80, 130, 255, alpha};
  default:
    return Color{255, 255, 255, alpha};
  }
}

// The histogram that matches a curve: luma for the master, else that channel.
inline const std::array<uint32_t, 256> &curveHistogram(CurveChannel ch) {
  switch (ch) {
  case CurveChannel::Red:
    return state.histR;
  case CurveChannel::Green:
    return state.histG;
  case CurveChannel::Blue:
    return state.histB;
  default:
    return state.histL;
  }
}

// Dark plot with the edited channel's histogram faintly behind it, a quarter
// grid, the identity diagonal, every non-straight curve (the others dimmed in
// their own color), and the edited curve's control points.
inline void paintCurve(CanvasContext &ctx) {
  const float W = ctx.width(), H = ctx.height();
  const float pad = kCurvePadPx, plot = curvePlotPx();
  ctx.setFillColor(Color{34, 34, 38, 255});
  ctx.fillRect(0, 0, W, H);

  const auto &hist = curveHistogram(uiState.curve.channel);
  uint32_t peak = 1;
  for (int i = 1; i < 255; ++i)
    peak = std::max(peak, hist[i]);
  const float invPeak = 1.0f / std::sqrt(static_cast<float>(peak));
  ctx.setFillColor(curveChannelColor(uiState.curve.channel, 45));
  for (int i = 0; i < 256; ++i) {
    const float hh =
        std::min(1.0f, std::sqrt(static_cast<float>(hist[i])) * invPeak) * plot;
    if (hh > 0.0f)
      ctx.fillRect(pad + static_cast<float>(i) / 256.0f * plot, pad + plot - hh,
                   plot / 256.0f + 0.5f, hh);
  }

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

  // Draws one curve as a polyline. Non-edited, non-straight curves first (dim),
  // so the edited curve ends up on top.
  auto strokeCurve = [&](CurveChannel ch, uint8_t alpha, float width) {
    const auto lut = curveToLut(state.adjust.curve(ch));
    ctx.setStrokeColor(curveChannelColor(ch, alpha));
    ctx.setLineWidth(width);
    ctx.beginPath();
    for (int i = 0; i < 256; ++i) {
      float lx, ly;
      curveToLocal(static_cast<float>(i), lut[i], lx, ly);
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
    if (ch != uiState.curve.channel && !(state.adjust.curve(ch) == straight))
      strokeCurve(ch, 110, 1.0f);
  }
  strokeCurve(uiState.curve.channel, 255, 1.75f);

  const CurvePoints &pts = curveActiveCurve();
  ctx.setLineWidth(1.0f);
  for (size_t i = 0; i < pts.size(); ++i) {
    float lx, ly;
    curveToLocal(pts[i].x, pts[i].y, lx, ly);
    const bool active = static_cast<int>(i) == uiState.curve.active;
    ctx.setFillColor(active ? kColorDividerHover : kColorWhite);
    ctx.setStrokeColor(Color{20, 20, 20, 255});
    ctx.fillRect(lx - kCurvePointPx / 2.0f, ly - kCurvePointPx / 2.0f,
                 kCurvePointPx, kCurvePointPx);
    ctx.strokeRect(lx - kCurvePointPx / 2.0f, ly - kCurvePointPx / 2.0f,
                   kCurvePointPx, kCurvePointPx);
  }
}