// app/masks.hpp: masks: selection, editing, overlay color, name field
#pragma once

#include <algorithm>  // std::clamp, std::max, std::min
#include <cstddef>    // size_t
#include <cstdint>    // uint8_t
#include <functional> // std::function
#include <optional>   // std::optional, std::nullopt
#include <string>     // std::string
#include <variant>    // std::get_if

#include "liteui.hpp"            // Color, TextInputState, kScrollbarThickness

#include "app/constants.hpp"     // kMasksActivity, kMaskPanelPad
#include "app/editor_state.hpp"  // inline EditorState state
#include "app/render_update.hpp" // updateImageFromAdjustments
#include "app/ui_state.hpp"      // uiState (.names, dropMaskCurveSelection)
#include "core/geometry.hpp"     // GeoFrame
#include "mask/mask.hpp"         // Mask, MaskSet, MaskPart, MaskType,
                                 // kMaxMaskParts, sanitizeMaskName
#include "mask/mask_brush.hpp"   // BrushShape
#include "mask/mask_geom.hpp"    // RadialShape, LinearShape
#include "mask/mask_polygon.hpp" // PolygonShape
#include "mask/mask_rect.hpp"    // RectShape

// ---------------- Masks: selection, editing, overlay ----------------
// The Masks pane (activity 7) edits `state.adjust.masks()`. Sliders go through
// buildSliderRow like everywhere else; the shape can also be dragged on the
// canvas (press inside = move, press a square handle = resize).
// Undo key of a mask edit: (mask * kMaxMaskParts + part) * kMaskCtlStride +
// control. Every control id of one mask must be different and below
// kMaskCtlStride, or two controls would fold into one undo step (a
// static_assert below checks it).
inline constexpr int kMaskCtlStride = 1000;
inline constexpr int kMaskCtlMove = 90; // undo-coalescing control ids (see maskEdit)
inline constexpr int kMaskCtlResizeX = 91;
inline constexpr int kMaskCtlResizeY = 92;
inline constexpr int kMaskCtlLinEnd = 93;
inline constexpr int kMaskCtlLinStart = 94;
inline constexpr int kMaskCtlBrush = 95; // one stroke = one undo step
inline constexpr int kMaskCtlLumLo = 96;
inline constexpr int kMaskCtlLumHi = 97;
inline constexpr int kMaskCtlPolyVertex =
    98; // a vertex placed / dragged = one undo step
inline constexpr int kMaskCtlPolyMove = 99;  // dragging the whole polygon
inline constexpr int kMaskCtlPolyLasso = 87; // one freehand lasso = one undo step
inline constexpr int kMaskCtlRange = 84;
inline constexpr int kMaskCtlFeather = 1;
inline constexpr int kMaskCtlAngle = 2;
inline constexpr int kMaskCtlWidth = 3;
inline constexpr int kMaskCtlHeight = 4;
inline constexpr int kMaskCtlLength = 5;
inline constexpr int kMaskCtlAdj =
    100; // + AdjId (100..100+Count-1, clear of every id below)
inline constexpr int kMaskCtlName = 86;

// ---- Mask overlay color ----
// The tint painted over the picture for the selected mask (radial / linear
// gradients, brush, luminosity, color range and combined bitmaps). One place,
// so every overlay agrees. Session-only: the choice is not saved anywhere.
struct MaskTintRgb {
  uint8_t r, g, b;
  const char *name;
};
inline constexpr MaskTintRgb kMaskTintPalette[] = {{255, 40, 40, "Red"},
                                            {60, 220, 90, "Green"},
                                            {70, 140, 255, "Blue"},
                                            {255, 210, 40, "Yellow"},
                                            {255, 255, 255, "White"}};
inline constexpr int kMaskTintCount =
    static_cast<int>(sizeof(kMaskTintPalette) / sizeof(kMaskTintPalette[0]));

inline const MaskTintRgb &maskTint() {
  const int i = std::clamp(state.maskTintColor, 0, kMaskTintCount - 1);
  return kMaskTintPalette[i];
}
// The overlay color with the given alpha.
inline Color maskTintColor(uint8_t alpha) {
  const MaskTintRgb &t = maskTint();
  return Color{t.r, t.g, t.b, alpha};
}
// Picks the overlay color. The cached tint bitmaps carry the old color, so they
// are dropped here; they are rebuilt on the next paint.
inline void setMaskTintColor(int idx) {
  state.maskTintColor = std::clamp(idx, 0, kMaskTintCount - 1);
  state.mask.tint.reset();
  state.mask.lumTint.reset();
  state.mask.compTint.reset();
  state.markDirty();
}

inline bool maskToolActive() {
  return state.activeActivity == kMasksActivity && state.image != nullptr;
}

// Width of one of `n` equal chips / tabs sharing a row of the mask panel.
inline float maskChipW(int n) {
  const float inner =
      state.maskPanelW - 2.0f * kMaskPanelPad - kScrollbarThickness;
  return std::max(20.0f, (inner - 4.0f * static_cast<float>(n - 1)) /
                             static_cast<float>(n));
}

// The selected mask's index, or -1. Clamped here instead of at every place
// that can shrink the list (undo, redo, open, delete).
inline int selMask() {
  const int n = static_cast<int>(state.adjust.masks().size());
  if (state.maskSel >= n)
    state.maskSel = n - 1;
  if (state.maskSel < -1)
    state.maskSel = -1;
  return state.maskSel;
}

// Selects mask `i` (-1 = none). The Curve tab's selected point belongs to the
// mask it was picked on, so it is dropped when another mask is selected.
// `force` = the list changed under the selection, so the same index can now
// be a different mask (delete); a plain click on the mask already selected
// keeps its point.
inline void selectMask(int i, bool force = false) {
  if (force || state.maskSel != i)
    uiState.dropMaskCurveSelection();
  state.maskSel = i;
}

inline const Mask *selectedMask() {
  const int i = selMask();
  return i < 0 ? nullptr : &state.adjust.masks()[static_cast<size_t>(i)];
}

// The mask's radial shape as it sits in the picture being shown (see the
// "masks" block: masks are STORED against the original picture). Everything
// on the canvas — outlines, hit tests, drags — works on this form.
// The selected mask's part that the canvas and the shape sliders work on. A
// mask can hold several parts (see MaskComposite); like selMask() this is
// clamped on read, so undo / delete / open never need to fix it up.
inline int selPart() {
  const Mask *m = selectedMask();
  if (!m || m->parts.empty())
    return 0;
  const int n = static_cast<int>(m->parts.size());
  if (state.mask.part >= n)
    state.mask.part = n - 1;
  if (state.mask.part < 0)
    state.mask.part = 0;
  return state.mask.part;
}
inline const MaskPart *selectedPart() {
  const Mask *m = selectedMask();
  if (!m || m->parts.empty())
    return nullptr;
  return &m->parts[static_cast<size_t>(selPart())];
}
inline MaskType selectedPartType() {
  const MaskPart *p = selectedPart();
  return p ? p->type() : MaskType::Count;
}
inline bool selectedIsBrush() { // the SELECTED PART is a brush
  return selectedPartType() == MaskType::Brush;
}
inline bool selectedIsPolygon() { // the SELECTED PART is a polygon / lasso
  return selectedPartType() == MaskType::Polygon;
}
// "Show mask tint": a brush that stands alone has no outline, so it keeps its
// own switch (on by default); everything else shares the common one.
inline bool &tintFlag() {
  const Mask *m = selectedMask();
  if (m && m->parts.size() == 1 && m->parts[0].type() == MaskType::Brush)
    return state.mask.brushTint;
  return state.maskShowTint;
}

// A part's shape as it sits in the picture being shown (see the "masks" block:
// masks are STORED against the original picture). Everything on the canvas —
// outlines, hit tests, drags — works on this form.
template <class Shape>
inline std::optional<Shape> shapeInView(const MaskPart &p, const GeoFrame &f) {
  if (const auto *r = std::get_if<Shape>(&p.shape))
    return r->toSource(f);
  return std::nullopt;
}
inline std::optional<RadialShape> radialInView(const MaskPart &p,
                                               const GeoFrame &f) {
  return shapeInView<RadialShape>(p, f);
}
inline std::optional<LinearShape> linearInView(const MaskPart &p,
                                               const GeoFrame &f) {
  return shapeInView<LinearShape>(p, f);
}
inline std::optional<BrushShape> brushInView(const MaskPart &p,
                                             const GeoFrame &f) {
  return shapeInView<BrushShape>(p, f);
}
inline std::optional<RectShape> rectInView(const MaskPart &p,
                                           const GeoFrame &f) {
  return shapeInView<RectShape>(p, f);
}
inline std::optional<PolygonShape> polygonInView(const MaskPart &p,
                                                 const GeoFrame &f) {
  return shapeInView<PolygonShape>(p, f);
}

// Edits part `part` of a mask on its in-view form and stores the result back
// (only the fields `fn` changed; see RadialShape::applySourceEdit).
template <class Shape>
inline void editShapeInView(Mask &m, size_t part, const GeoFrame &f,
                            const std::function<void(Shape &)> &fn) {
  if (part >= m.parts.size())
    return;
  auto *stored = std::get_if<Shape>(&m.parts[part].shape);
  if (!stored)
    return;
  const Shape before = stored->toSource(f);
  Shape after = before;
  fn(after);
  stored->applySourceEdit(before, after, f);
}
inline void editRadialInView(Mask &m, size_t part, const GeoFrame &f,
                             const std::function<void(RadialShape &)> &fn) {
  editShapeInView<RadialShape>(m, part, f, fn);
}
inline void editLinearInView(Mask &m, size_t part, const GeoFrame &f,
                             const std::function<void(LinearShape &)> &fn) {
  editShapeInView<LinearShape>(m, part, f, fn);
}

// Edits mask `idx` in a copy of the list and commits it. `control` >= 0 names
// the control being dragged: repeated edits of the same control of the same
// mask (and, for the selected mask, the same part) fold into one undo step
// (see AdjustmentSet::setMasks). -1 = discrete.
// Does NOT recompute the picture — slider / canvas drags do that through the
// usual markAdjustDirty path; discrete actions call maskCommitDiscrete.
inline void maskEdit(int idx, const std::function<void(Mask &)> &fn,
                     int control) {
  MaskSet ms = state.adjust.masks();
  if (idx < 0 || idx >= static_cast<int>(ms.size()))
    return;
  const int part = idx == selMask() ? selPart() : 0;
  fn(ms[static_cast<size_t>(idx)]);
  state.adjust.setMasks(ms, control < 0
                                ? -1
                                : (idx * static_cast<int>(kMaxMaskParts) +
                                   part) * kMaskCtlStride +
                                      control);
}

// A one-off change to the selected mask (toggle, reset): its own undo step and
// an immediate recompute.
inline void maskEditDiscrete(const std::function<void(Mask &)> &fn) {
  const int i = selMask();
  if (i < 0)
    return;
  maskEdit(i, fn, -1);
  updateImageFromAdjustments();
  state.markDirty();
}

// ---- Mask name field ----
// The TextInput keeps its text in a shared state; we hold a pointer to it so
// the field can follow the selected mask (selection change, undo / redo).

// True while a name field has keyboard focus: the global shortcuts that
// would also fire on plain keys (Y, J, Left, Right, Delete...) must stand
// down, or typing a name would toggle views and Delete would delete the mask.
inline bool typingInTextField() {
  return (uiState.names.mask && uiState.names.mask->focused) ||
         (uiState.names.preset && uiState.names.preset->focused) ||
         (uiState.names.snapshot && uiState.names.snapshot->focused);
}

// Makes the field show the selected mask's name. Cheap; polled from the
// field's display lambda, so it also catches undo / redo.
inline void syncMaskNameField() {
  if (!uiState.names.mask)
    return;
  const Mask *m = selectedMask();
  const std::string want = m ? m->name : std::string();
  TextInputState &t = *uiState.names.mask;
  if (t.text == want)
    return;
  t.text = want;
  t.cursor = want.size();
  t.scrollOffset = 0.0f;
  t.dirty = true;
}

inline void renameSelectedMask(const std::string &typed) {
  const std::string s = sanitizeMaskName(typed);
  if (uiState.names.mask &&
      uiState.names.mask->text !=
          s) { // the cap (or a stripped character) changed what was typed
    uiState.names.mask->text = s;
    uiState.names.mask->cursor = std::min(uiState.names.mask->cursor, s.size());
    uiState.names.mask->dirty = true;
  }
  // Typing folds into one undo step (until Enter / another edit).
  maskEdit(selMask(), [&](Mask &m) { m.name = s; }, kMaskCtlName);
  state.markDirty();
}

