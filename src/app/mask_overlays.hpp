// app/mask_overlays.hpp: brush, luminosity and combined mask overlays
#pragma once

#include <algorithm> // std::min, std::max, std::clamp
#include <cmath>     // std::lround
#include <cstddef>   // size_t
#include <cstdint>   // uint8_t
#include <memory>    // std::shared_ptr, std::weak_ptr, std::make_shared
#include <utility>   // std::move
#include <variant>   // std::holds_alternative, std::get
#include <vector>    // std::vector

#include "liteui.hpp"             // CanvasImage, CanvasContext, Color

#include "app/constants.hpp"      // kMaskHandlePx, kColorWhite, kBrushTintEdge,
                                  // kLumTintEdge, kCompTintEdge
#include "app/editor_state.hpp"   // inline EditorState state, MaskUi
#include "app/mask_brush.hpp"     // paintRadialTint, paintRadialOutline,
                                  // paintLinearTint, paintLinearOutline,
                                  // paintRectOutline, paintPolygonOutline
#include "app/masks.hpp"          // selMask, selPart, selectedPart, selectedIsBrush,
                                  // maskTint, shapeInView, radialInView,
                                  // linearInView, brushInView, rectInView,
                                  // polygonInView
#include "core/geometry.hpp"      // GeoFrame
#include "edit/render_params.hpp" // RenderParams, buildMaskPasses
#include "mask/mask.hpp"          // Mask, MaskSet, MaskPart, MaskPassPart,
                                  // MaskComposite, prepareComposite,
                                  // maskTypeUsesPixels, maskShapeToSource
#include "mask/mask_brush.hpp"    // BrushShape, BrushEval, BrushPt
#include "mask/mask_object.hpp"   // ObjectShape, ObjectPrompt
#include "mask/mask_range.hpp"    // LuminosityShape, LuminosityEval, ColorRangeShape
#include "render/render_core.hpp" // renderWith

// ---- Brush overlay ----
// The red coverage bitmap: the brush rasterized at a small size through the
// SAME BrushShape::prepare the renderer uses, so what is tinted is exactly what
// gets adjusted. Cached until the strokes or the picture's size change (the
// shape is compared, which is cheap next to rasterizing it).
inline std::shared_ptr<CanvasImage> brushTintImage(const BrushShape &b,
                                                   bool invert) {
  const float W = state.docWidth(), H = state.docHeight();
  const float sc =
      std::min(1.0f, static_cast<float>(kBrushTintEdge) / std::max(W, H));
  const int tw = std::max(1, static_cast<int>(std::lround(W * sc)));
  const int th = std::max(1, static_cast<int>(std::lround(H * sc)));
  MaskUi &u = state.mask;
  if (u.tint && u.tint->width == tw && u.tint->height == th &&
      u.tintInvert == invert && u.tintShape == b)
    return u.tint;
  const BrushEval ev = b.prepare(tw, th);
  auto img = std::make_shared<CanvasImage>(tw, th);
  std::vector<float> wb(static_cast<size_t>(tw));
  for (int y = 0; y < th; ++y) {
    ev.rowWeights(y, 0, tw, wb.data());
    uint8_t *row = img->pixels.data() +
                   static_cast<size_t>(y) * static_cast<size_t>(tw) * 4;
    for (int x = 0; x < tw; ++x) {
      float w = wb[static_cast<size_t>(x)];
      if (invert)
        w = 1.0f - w;
      uint8_t *p = row + static_cast<size_t>(x) * 4;
      p[0] = maskTint().r;
      p[1] = maskTint().g;
      p[2] = maskTint().b;
      p[3] = static_cast<uint8_t>(std::lround(120.0f * w));
    }
  }
  u.tint = img;
  u.tintShape = b;
  u.tintInvert = invert;
  return img;
}

inline void paintBrushTint(CanvasContext &ctx, const BrushShape &b,
                           bool invert) {
  if (b.strokes.empty() && !invert)
    return; // nothing painted yet
  const auto img = brushTintImage(b, invert);
  ctx.drawImage(*img, 0, 0, state.docWidth(), state.docHeight());
}

// The pin marks where a brush mask's first stroke began (all a brush has
// for an "outline"); the selected one is drawn solid.
inline void paintBrushPin(CanvasContext &ctx, const BrushShape &b,
                          bool selected) {
  if (b.strokes.empty() || b.strokes.front().pts.empty())
    return;
  const BrushPt &p = b.strokes.front().pts.front();
  const float px = 1.0f / state.zoom; // one screen pixel, in image units
  constexpr float kTwoPi = 6.28318530717959f;
  const float cx = p.x * state.docWidth(), cy = p.y * state.docHeight();
  const float r = kMaskHandlePx * px * 0.55f;
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0.0f, kTwoPi);
  ctx.setFillColor(Color{255, 255, 255, selected ? uint8_t{200} : uint8_t{90}});
  ctx.fill();
  ctx.setStrokeColor(Color{0, 0, 0, selected ? uint8_t{200} : uint8_t{120}});
  ctx.setLineWidth(px * 1.5f);
  ctx.stroke();
}

// The brush's footprint at the pointer while painting: the rim, and where the
// feather begins.
inline void paintBrushCursor(CanvasContext &ctx) {
  const MaskUi &u = state.mask;
  const float px = 1.0f / state.zoom;
  constexpr float kTwoPi = 6.28318530717959f;
  const float r = u.brushSize * std::min(state.docWidth(), state.docHeight());
  ctx.beginPath();
  ctx.arc(u.curX, u.curY, r, 0.0f, kTwoPi);
  ctx.setStrokeColor(Color{0, 0, 0, 150});
  ctx.setLineWidth(px * 3.0f);
  ctx.stroke();
  ctx.setStrokeColor(kColorWhite);
  ctx.setLineWidth(px * 1.5f);
  ctx.stroke();
  const float inner = std::clamp(1.0f - u.brushFeather, 0.0f, 0.99f);
  if (u.brushFeather > 0.02f && inner > 0.02f) {
    ctx.beginPath();
    ctx.arc(u.curX, u.curY, r * inner, 0.0f, kTwoPi);
    ctx.setLineDash({4.0f * px, 4.0f * px});
    ctx.setStrokeColor(Color{255, 255, 255, 170});
    ctx.setLineWidth(px);
    ctx.stroke();
    ctx.setLineDash({});
  }
}

// ---- Luminosity overlay ----
inline bool sameOwner(const std::weak_ptr<CanvasImage> &a,
                      const std::shared_ptr<CanvasImage> &b) {
  return !a.owner_before(b) && !b.owner_before(a);
}

// state.source box-downsampled by an integer factor to at most
// kLumTintEdge on its long side (the source itself when already that small).
inline std::shared_ptr<CanvasImage> lumTintBase() {
  const std::shared_ptr<CanvasImage> src = state.source;
  if (!src || src->width <= 0 || src->height <= 0)
    return nullptr;
  MaskUi &u = state.mask;
  if (u.lumBase && sameOwner(u.lumBaseSrc, src))
    return u.lumBase;
  const int longEdge = std::max(src->width, src->height);
  const int k = std::max(1, (longEdge + kLumTintEdge - 1) / kLumTintEdge);
  std::shared_ptr<CanvasImage> base = src;
  if (k > 1) {
    const int w = src->width / k, h = src->height / k;
    if (w <= 0 || h <= 0)
      return nullptr;
    auto out = std::make_shared<CanvasImage>(w, h);
    const int area = k * k;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        int sum[4] = {0, 0, 0, 0};
        for (int dy = 0; dy < k; ++dy) {
          const uint8_t *row =
              &src->pixels[(static_cast<size_t>(y * k + dy) * src->width +
                            static_cast<size_t>(x * k)) *
                           4];
          for (int dx = 0; dx < k; ++dx)
            for (int c = 0; c < 4; ++c)
              sum[c] += row[dx * 4 + c];
        }
        uint8_t *o = &out->pixels[(static_cast<size_t>(y) * w + x) * 4];
        for (int c = 0; c < 4; ++c)
          o[c] = static_cast<uint8_t>((sum[c] + area / 2) / area);
      }
    base = std::move(out);
  }
  u.lumBase = base;
  u.lumBaseSrc = src;
  return base;
}

// What the mask selects, as a red bitmap. The weights come from the SAME
// LuminosityShape::prepare the renderer uses, run over the small picture as
// it stands just before this mask: the global adjustments plus the masks
// listed above it, but not this mask's own edits (which cannot change what it
// selects). Cached until the shape, the picture's pixels or the mask's place in
// the list change.
inline std::shared_ptr<CanvasImage>
luminosityTintImage(const LuminosityShape &s, bool invert, int sel) {
  MaskUi &u = state.mask;
  if (u.lumTint && u.lumSel == sel && u.lumInvert == invert &&
      u.lumShape == s && sameOwner(u.lumKeyImage, state.image) &&
      sameOwner(u.lumKeyPreview, state.previewImage))
    return u.lumTint;
  const MaskSet &ms = state.adjust.masks();
  const auto base = lumTintBase();
  if (!base || sel < 0 || sel >= static_cast<int>(ms.size()))
    return nullptr;
  RenderParams rp = state.adjust.buildRenderParams();
  rp.film = {}; // the picture BEFORE the mask: no Vignette / Grain (they run
                // after the masks)
  rp.masks = buildMaskPasses(MaskSet(ms.begin(), ms.begin() + sel),
                             state.adjust.geoFrame());
  const auto pre = renderWith(*base, rp);
  const int tw = pre->width, th = pre->height;
  const LuminosityEval ev = s.prepare(tw, th);
  auto img = std::make_shared<CanvasImage>(tw, th);
  std::vector<float> wb(static_cast<size_t>(tw));
  for (int y = 0; y < th; ++y) {
    const size_t off = static_cast<size_t>(y) * static_cast<size_t>(tw) * 4;
    ev.rowWeights(y, 0, tw, wb.data(), pre->pixels.data() + off);
    uint8_t *row = img->pixels.data() + off;
    for (int x = 0; x < tw; ++x) {
      float w = wb[static_cast<size_t>(x)];
      if (invert)
        w = 1.0f - w;
      uint8_t *p = row + static_cast<size_t>(x) * 4;
      p[0] = maskTint().r;
      p[1] = maskTint().g;
      p[2] = maskTint().b;
      p[3] = static_cast<uint8_t>(std::lround(120.0f * w));
    }
  }
  u.lumTint = img;
  u.lumShape = s;
  u.lumInvert = invert;
  u.lumSel = sel;
  u.lumKeyImage = state.image;
  u.lumKeyPreview = state.previewImage;
  return img;
}

inline void paintLuminosityTint(CanvasContext &ctx, const LuminosityShape &s,
                                bool invert, int sel) {
  if (const auto img = luminosityTintImage(s, invert, sel))
    ctx.drawImage(*img, 0, 0, state.docWidth(), state.docHeight());
}

// ---- Combined mask overlay ----
// The red bitmap of a selected mask with SEVERAL parts: the whole combination,
// evaluated by the same MaskComposite the renderer uses, on a small picture.
// If any part is a luminosity range the picture is the one just BEFORE this
// mask (see luminosityTintImage); otherwise the weights need no pixels and
// the bitmap is sized from the document. Cached on the parts in their in-view
// form, so a rotate / crop / edit all rebuild it.
inline std::shared_ptr<CanvasImage> compositeTintImage(const Mask &m, int sel) {
  MaskUi &u = state.mask;
  const GeoFrame frame = state.adjust.geoFrame();
  bool usesPix = false;
  std::vector<MaskPart> view;
  view.reserve(m.parts.size());
  for (const MaskPart &p : m.parts) {
    if (maskTypeUsesPixels(p.type()))
      usesPix = true;
    view.push_back(MaskPart{maskShapeToSource(p.shape, frame), p.op});
  }
  int tw = 0, th = 0;
  if (!usesPix) {
    const float W = state.docWidth(), H = state.docHeight();
    const float sc =
        std::min(1.0f, static_cast<float>(kCompTintEdge) / std::max(W, H));
    tw = std::max(1, static_cast<int>(std::lround(W * sc)));
    th = std::max(1, static_cast<int>(std::lround(H * sc)));
  }
  if (u.compTint && u.compSel == sel && u.compInvert == m.invert &&
      u.compUsesPix == usesPix && u.compParts == view &&
      (usesPix ? (sameOwner(u.compKeyImage, state.image) &&
                  sameOwner(u.compKeyPreview, state.previewImage))
               : (u.compW == tw && u.compH == th)))
    return u.compTint;

  std::shared_ptr<CanvasImage> pre;
  if (usesPix) {
    const MaskSet &ms = state.adjust.masks();
    const auto base = lumTintBase();
    if (!base || sel < 0 || sel >= static_cast<int>(ms.size()))
      return nullptr;
    RenderParams rp = state.adjust.buildRenderParams();
    rp.film = {}; // the picture BEFORE the mask: no Vignette / Grain (they run
                  // after the masks)
    rp.masks = buildMaskPasses(MaskSet(ms.begin(), ms.begin() + sel), frame);
    pre = renderWith(*base, rp);
    tw = pre->width;
    th = pre->height;
  }
  std::vector<MaskPassPart> pp;
  pp.reserve(view.size());
  for (const MaskPart &p : view)
    pp.push_back(MaskPassPart{p.shape, p.op});
  const MaskComposite comp = prepareComposite(pp, tw, th);
  auto img = std::make_shared<CanvasImage>(tw, th);
  std::vector<float> wb(static_cast<size_t>(tw)), wb2(static_cast<size_t>(tw));
  for (int y = 0; y < th; ++y) {
    const size_t off = static_cast<size_t>(y) * static_cast<size_t>(tw) * 4;
    comp.rowWeights(y, 0, tw, wb.data(),
                    pre ? pre->pixels.data() + off : nullptr, wb2.data());
    uint8_t *row = img->pixels.data() + off;
    for (int x = 0; x < tw; ++x) {
      float w = wb[static_cast<size_t>(x)];
      if (m.invert)
        w = 1.0f - w;
      uint8_t *p = row + static_cast<size_t>(x) * 4;
      p[0] = maskTint().r;
      p[1] = maskTint().g;
      p[2] = maskTint().b;
      p[3] = static_cast<uint8_t>(std::lround(120.0f * w));
    }
  }
  u.compTint = img;
  u.compParts = std::move(view);
  u.compInvert = m.invert;
  u.compUsesPix = usesPix;
  u.compSel = sel;
  u.compW = tw;
  u.compH = th;
  u.compKeyImage = state.image;
  u.compKeyPreview = state.previewImage;
  return img;
}

inline void paintCompositeTint(CanvasContext &ctx, const Mask &m, int sel) {
  if (const auto img = compositeTintImage(m, sel))
    ctx.drawImage(*img, 0, 0, state.docWidth(), state.docHeight());
}

inline void paintSelectedObjectPrompts(CanvasContext &ctx) {
  const MaskPart *part = selectedPart();
  if (!part || !std::holds_alternative<ObjectShape>(part->shape))
    return;
  const auto &stored = std::get<ObjectShape>(part->shape);
  const auto &pend = state.mask.objPending;
  const std::vector<ObjectPrompt> &src =
      (pend.mask == selMask() && pend.part == selPart()) ? pend.prompts
                                                         : stored.prompts;
  const GeoFrame f = state.adjust.geoFrame();
  const float W = state.docWidth(), H = state.docHeight();
  const float px = 1.0f / state.zoom;
  for (const ObjectPrompt &p : src) {
    double sx, sy;
    f.toSrc(p.x * f.origW, p.y * f.origH, sx, sy);
    const float cx = static_cast<float>(sx / f.srcW) * W;
    const float cy = static_cast<float>(sy / f.srcH) * H;
    ctx.beginPath();
    ctx.arc(cx, cy, 5.0f * px, 0.0f, 6.28318530717959f);
    ctx.setFillColor(p.fg ? Color{60, 220, 90, 255} : Color{80, 150, 255, 255});
    ctx.fill();
    ctx.setStrokeColor(kColorWhite);
    ctx.setLineWidth(1.5f * px);
    ctx.stroke();
  }
}

// A part's outline / handles / pin (nothing for a luminosity range).
inline void paintPartOutline(CanvasContext &ctx, const MaskPart &p,
                             const GeoFrame &frame, bool selected) {
  if (const auto r = radialInView(p, frame))
    paintRadialOutline(ctx, *r, selected);
  else if (const auto l = linearInView(p, frame))
    paintLinearOutline(ctx, *l, selected);
  else if (const auto b = brushInView(p, frame))
    paintBrushPin(ctx, *b, selected);
  else if (const auto q = rectInView(p, frame))
    paintRectOutline(ctx, *q, selected);
  else if (const auto pg = polygonInView(p, frame))
    paintPolygonOutline(ctx, *pg, selected);
}

inline void paintMaskOverlay(CanvasContext &ctx) {
  const MaskSet &ms = state.adjust.masks();
  const int sel = selMask();
  const int selP = selPart();
  const GeoFrame frame = state.adjust.geoFrame();
  // Unselected masks first (faint), the selected one on top.
  for (int i = 0; i < static_cast<int>(ms.size()); ++i) {
    if (i == sel)
      continue;
    for (const MaskPart &p : ms[static_cast<size_t>(i)].parts)
      paintPartOutline(ctx, p, frame, false);
  }
  if (sel < 0)
    return;
  const Mask &m = ms[static_cast<size_t>(sel)];
  if (m.parts.empty())
    return;
  if (m.parts.size() > 1) {
    // Several parts: tint the combination, then the parts' own outlines
    // (the selected part's with its handles).
    if (state.maskShowTint)
      paintCompositeTint(ctx, m, sel);
    for (int j = 0; j < static_cast<int>(m.parts.size()); ++j)
      if (j != selP)
        paintPartOutline(ctx, m.parts[static_cast<size_t>(j)], frame, false);
    paintPartOutline(ctx, m.parts[static_cast<size_t>(selP)], frame, true);
    paintSelectedObjectPrompts(ctx);
    if (state.mask.drag == MaskUi::Drag::Paint && selectedIsBrush())
      paintBrushCursor(ctx);
    return;
  }
  const MaskPart &p = m.parts[0];
  if (const auto r = radialInView(p, frame)) {
    if (state.maskShowTint)
      paintRadialTint(ctx, *r, m.invert);
    paintRadialOutline(ctx, *r, true);
  } else if (const auto l = linearInView(p, frame)) {
    if (state.maskShowTint)
      paintLinearTint(ctx, *l, m.invert);
    paintLinearOutline(ctx, *l, true);
  } else if (const auto b = brushInView(p, frame)) {
    if (state.mask.brushTint)
      paintBrushTint(ctx, *b, m.invert);
    paintBrushPin(ctx, *b, true);
    if (state.mask.drag == MaskUi::Drag::Paint)
      paintBrushCursor(ctx);
  } else if (const auto lm = shapeInView<LuminosityShape>(p, frame)) {
    // No outline or handles: the tint is the whole overlay.
    if (state.maskShowTint)
      paintLuminosityTint(ctx, *lm, m.invert, sel);
  } else if (std::holds_alternative<ObjectShape>(p.shape)) {
    if (state.maskShowTint)
      paintCompositeTint(ctx, m, sel);
    paintSelectedObjectPrompts(ctx);
  } else if (std::holds_alternative<ColorRangeShape>(p.shape)) {
    // Not spatial: the tint (built from the picture before this mask) is
    // the whole overlay.
    if (state.maskShowTint)
      paintCompositeTint(ctx, m, sel);
  } else if (const auto q = rectInView(p, frame)) {
    if (state.maskShowTint)
      paintCompositeTint(ctx, m, sel);
    paintRectOutline(ctx, *q, true);
  } else if (const auto pg = polygonInView(p, frame)) {
    if (state.maskShowTint)
      paintCompositeTint(ctx, m, sel);
    paintPolygonOutline(ctx, *pg, true);
  }
}

