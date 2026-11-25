// app/mask_list.hpp: mask list operations and parts
#pragma once

#include <cstddef> // size_t, std::ptrdiff_t
#include <string>  // std::string, std::to_string
#include <utility> // std::swap

#include "app/editor_state.hpp"   // inline EditorState state, MaskUi, setStatus
#include "app/masks.hpp"          // selMask, selPart, selectedMask, selectMask, maskEditDiscrete
#include "app/render_update.hpp"  // updateImageFromAdjustments
#include "app/ui_state.hpp"       // inline UiState uiState, MaskEffects
#include "app/virtual_copies.hpp" // newMaskShape, objectPrefetch
#include "mask/mask.hpp"          // Mask, MaskSet, MaskPart, MaskOp, MaskType, limits

// ---- Mask list operations: duplicate, reorder, copy / paste effects ----
// A copy of the selected mask (shapes and effects) right after it, selected.
inline void duplicateSelectedMask() {
  const int i = selMask();
  if (i < 0)
    return;
  MaskSet ms = state.adjust.masks();
  if (ms.size() >= kMaxMasks) {
    setStatus("Mask limit reached (" + std::to_string(kMaxMasks) + ")", true);
    return;
  }
  Mask copy = ms[static_cast<size_t>(i)];
  if (!maskNameBlank(copy.name))
    copy.name = sanitizeMaskName(copy.name + " copy");
  ms.insert(ms.begin() + i + 1, copy);
  state.adjust.setMasks(ms, -1);
  selectMask(i + 1);
  state.mask.part = 0;
  updateImageFromAdjustments();
  state.markDirty();
}

// Masks apply in list order (later ones on top, and a luminosity range sees
// the result of the masks before it), so order matters. dir = -1 up, +1 down.
inline void moveSelectedMask(int dir) {
  const int i = selMask();
  const int j = i + dir;
  MaskSet ms = state.adjust.masks();
  if (i < 0 || j < 0 || j >= static_cast<int>(ms.size()))
    return;
  std::swap(ms[static_cast<size_t>(i)], ms[static_cast<size_t>(j)]);
  state.adjust.setMasks(ms, -1);
  selectMask(j);
  updateImageFromAdjustments();
  state.markDirty();
}

inline void copyMaskEffects() {
  const Mask *m = selectedMask();
  if (!m)
    return;
  uiState.clipboard.maskEffects =
      MaskEffects{m->amount, m->adj, m->mix, m->grade, m->curves};
  setStatus("Mask effects copied", false);
}

inline void pasteMaskEffects() {
  if (!uiState.clipboard.maskEffects) {
    setStatus("Copy a mask's effects first", true);
    return;
  }
  const MaskEffects fx = *uiState.clipboard.maskEffects;
  maskEditDiscrete([&](Mask &m) {
    m.amount = fx.amount;
    m.adj = fx.adj;
    m.mix = fx.mix;
    m.grade = fx.grade;
    m.curves = fx.curves;
  });
}

// ---- Parts: combining shapes inside the selected mask ----
// Adds a part of kind `t` (joined with Add; change it with setPartOp) and
// selects it. The combined result is what matters from here on, so the tint
// is switched on.
inline void addPart(MaskType t) {
  const int i = selMask();
  if (i < 0)
    return;
  const auto shape = newMaskShape(t);
  if (!shape)
    return;
  MaskSet ms = state.adjust.masks();
  Mask &m = ms[static_cast<size_t>(i)];
  if (m.parts.size() >= kMaxMaskParts) {
    setStatus("Part limit reached (" + std::to_string(kMaxMaskParts) + ")",
              true);
    return;
  }
  m.parts.push_back(MaskPart{*shape, MaskOp::Add});
  state.adjust.setMasks(ms, -1);
  state.mask.part = static_cast<int>(m.parts.size()) - 1;
  state.maskShowTint = true;
  updateImageFromAdjustments();
  state.markDirty();

  if (t == MaskType::Object)
    objectPrefetch();
}

// How the selected part joins the ones before it (the first part has none).
inline void setPartOp(MaskOp op) {
  const size_t j = static_cast<size_t>(selPart());
  if (j == 0)
    return;
  maskEditDiscrete([&](Mask &m) {
    if (j < m.parts.size())
      m.parts[j].op = op;
  });
}

inline void removeSelectedPart() {
  const Mask *sm = selectedMask();
  if (!sm || sm->parts.size() <= 1)
    return;
  const size_t j = static_cast<size_t>(selPart());
  maskEditDiscrete([&](Mask &m) {
    if (m.parts.size() > 1 && j < m.parts.size())
      m.parts.erase(m.parts.begin() + static_cast<std::ptrdiff_t>(j));
  });
  state.mask.part = static_cast<int>(j) - 1; // the one before; selPart clamps
  state.markDirty();
}

// A drag on the canvas or a mask slider ended: leave the low-res preview and
// ask for one full-resolution render (same job as gradeEndDrag).
inline void maskEndDrag() {
  state.mask.drag = MaskUi::Drag::None;
  state.adjust.endMaskEdit();
  if (state.dragging) {
    state.dragging = false;
    state.dragIsAngle = false;
    state.markAdjustDirty();
  }
}

