// app/mask_widgets.hpp: Masks pane widgets (also the preset / LUT / snapshot rows)
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---- Masks pane widgets ----

// What a mask is called in the list: its first part's kind, then the next
// one, then how many more ("Radial + Brush +1").
inline std::string maskLabel(const Mask &m) {
  if (m.parts.empty())
    return "Empty";
  if (!maskNameBlank(m.name))
    return m.name;
  std::string s = maskTypeName(m.parts[0].type());
  if (m.parts.size() >= 2)
    s += std::string(" + ") + maskTypeName(m.parts[1].type());
  if (m.parts.size() > 2)
    s += " +" + std::to_string(m.parts.size() - 2);
  return s;
}

// One row of the mask list: "1  Radial", highlighted when selected. There are
// kMaxMasks of these, shown only for the masks that exist (the view tree is
// built once, so the list is a fixed set of slots).
inline View buildMaskRow(size_t i) {
  View b;
  b.style.width = Size::full();
  b.style.height = Size::pixel(28);
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.alignItems = Align::Center;
  b.style.padding = EdgeInsets{0, 10, 0, 10};
  b.style.hoverColor = kColorActivityHover;
  b.style.display = std::function<Display()>([i] {
    return i < state.adjust.masks().size() ? Display::Flex : Display::None;
  });
  const auto isSel = [i] { return selMask() == static_cast<int>(i); };
  b.style.backgroundColor = std::function<Color()>([isSel]() -> Color {
    return isSel() ? kColorDropdownRowHover : kColorActivityBarBg;
  });
  b.style.borderWidth =
      std::function<float()>([isSel] { return isSel() ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  b.onClick = [i] {
    if (state.maskSel != static_cast<int>(i))
      state.mask.part = 0; // a different mask: start on its first part
    selectMask(static_cast<int>(i));
    state.markDirty(); // move the overlay to it
  };

  Text t;
  t.label = std::function<std::string()>([i]() -> std::string {
    const MaskSet &ms = state.adjust.masks();
    if (i >= ms.size())
      return std::string();
    const Mask &m = ms[i];
    std::string s = std::to_string(i + 1) + "   " + maskLabel(m);
    if (m.invert)
      s += "  (inverted)";
    if (!m.enabled)
      s += "  (off)";
    return s;
  });
  t.fontSize = 13.0f;
  t.color = kColorTextDark;
  b.addChild(std::move(t));
  return b;
}

// One row of the Presets list. Click applies the preset (undoable).
inline View buildPresetRow(size_t i) {
  View b;
  b.style.width = Size::full();
  b.style.height = Size::pixel(28);
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.alignItems = Align::Center;
  b.style.padding = EdgeInsets{0, 10, 0, 10};
  b.style.overflowX = Overflow::Hidden;
  b.style.hoverColor = kColorActivityHover;
  b.style.display = std::function<Display()>([i] {
    return state.activeActivity == kPresetsActivity && i < uiState.presets.list.size()
               ? Display::Flex
               : Display::None;
  });
  const auto isSel = [i] { return uiState.presets.sel == static_cast<int>(i); };
  b.style.backgroundColor = std::function<Color()>([isSel]() -> Color {
    return isSel() ? kColorDropdownRowHover : kColorActivityBarBg;
  });
  b.style.borderWidth =
      std::function<float()>([isSel] { return isSel() ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  b.onClick = [i] { applyPresetEntry(i); };

  Text t;
  t.label = std::function<std::string()>([i]() -> std::string {
    return i < uiState.presets.list.size() ? uiState.presets.list[i].name : std::string();
  });
  t.fontSize = 13.0f;
  t.wrap = TextWrap::NoWrap;
  t.color = kColorTextDark;
  b.addChild(std::move(t));
  return b;
}

// One row of the LUTs list: the open picture under that LUT, the LUT's name and
// a line about the file. Click applies it (undoable). With no picture open a
// row is just the name.
inline View buildLutRow(size_t i) {
  const auto withPicture = [] { return state.original != nullptr; };
  const auto isCur = [i] {
    const LutRef &r = state.adjust.lut();
    return i < gLuts.size() && !r.empty() &&
           filmLower(r.name) == filmLower(gLuts[i].entry.name);
  };

  View b;
  b.style.width = Size::full();
  b.style.height = std::function<Size()>([withPicture] {
    return Size::pixel(withPicture() ? kLutRowH : kLutRowNamesH);
  });
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.direction = FlexDirection::Row;
  b.style.alignItems = Align::Center;
  b.style.gap = 10.0f;
  b.style.padding = EdgeInsets{0, 10, 0, 6};
  b.style.overflowX = Overflow::Hidden;
  b.style.hoverColor = kColorActivityHover;
  b.style.display = std::function<Display()>([i] {
    return state.activeActivity == kLutsActivity && i < gLuts.size()
               ? Display::Flex
               : Display::None;
  });
  b.style.backgroundColor = std::function<Color()>([isCur]() -> Color {
    return isCur() ? kColorDropdownRowHover : kColorActivityBarBg;
  });
  b.style.borderWidth =
      std::function<float()>([isCur] { return isCur() ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  b.onClick = [i] { applyLutRow(i); };

  // The preview. A click on it bubbles up to the row.
  View thumb;
  thumb.isCanvas = true;
  thumb.style.width = Size::pixel(kFilmThumbW);
  thumb.style.height = Size::pixel(kFilmThumbH);
  thumb.style.flexShrink = 0;
  thumb.style.display = std::function<Display()>(
      [withPicture] { return withPicture() ? Display::Flex : Display::None; });
  thumb.onPaint = [i](CanvasContext &ctx) {
    ctx.setFillColor(kColorTrack);
    ctx.fillRect(0, 0, kFilmThumbW, kFilmThumbH);
    if (i < gLuts.size() && gLuts[i].thumb) {
      const CanvasImage &img = *gLuts[i].thumb;
      ctx.drawImage(img, std::floor((kFilmThumbW - img.width) / 2.0f),
                    std::floor((kFilmThumbH - img.height) / 2.0f));
    }
  };
  // Repaint when this row's picture changed, or when the canvas has just been
  // shown again.
  auto last = std::make_shared<std::array<uint64_t, 3>>(
      std::array<uint64_t, 3>{~uint64_t{0}, ~uint64_t{0}, ~uint64_t{0}});
  thumb.canvasDirtySource = [i, last, withPicture]() -> bool {
    const std::array<uint64_t, 3> now{
        i < gLuts.size() ? gLuts[i].paintVersion : 0, withPicture() ? 1u : 0u,
        state.activeActivity == kLutsActivity ? 1u : 0u};
    if (now == *last)
      return false;
    *last = now;
    return true;
  };
  b.addChild(std::move(thumb));

  View col;
  col.style.direction = FlexDirection::Column;
  col.style.flexGrow = 1;
  col.style.gap = 2.0f;
  col.style.overflowX = Overflow::Hidden;

  Text name;
  name.label = std::function<std::string()>([i]() -> std::string {
    return i < gLuts.size() ? gLuts[i].entry.name : std::string();
  });
  name.fontSize = 13.0f;
  name.wrap = TextWrap::NoWrap;
  name.color = kColorTextDark;
  col.addChild(std::move(name));

  Text detail;
  detail.label = std::function<std::string()>(
      [i]() -> std::string { return lutRowDetail(i); });
  detail.fontSize = 11.0f;
  detail.wrap = TextWrap::NoWrap;
  detail.color = kColorTextMuted;
  col.addChild(std::move(detail));

  b.addChild(std::move(col));
  return b;
}

// One row of the Snapshots list. Click restores the snapshot (undoable).
inline View buildSnapshotRow(size_t i) {
  View b;
  b.style.width = Size::full();
  b.style.height = Size::pixel(28);
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.alignItems = Align::Center;
  b.style.padding = EdgeInsets{0, 10, 0, 10};
  b.style.overflowX = Overflow::Hidden;
  b.style.hoverColor = kColorActivityHover;
  b.style.display = std::function<Display()>([i] {
    return state.activeActivity == kSnapshotsActivity &&
                   i < state.snapshots.size()
               ? Display::Flex
               : Display::None;
  });
  const auto isSel = [i] { return uiState.snapshots.sel == static_cast<int>(i); };
  b.style.backgroundColor = std::function<Color()>([isSel]() -> Color {
    return isSel() ? kColorDropdownRowHover : kColorActivityBarBg;
  });
  b.style.borderWidth =
      std::function<float()>([isSel] { return isSel() ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  b.onClick = [i] { applySnapshotEntry(i); };

  Text t;
  t.label = std::function<std::string()>([i]() -> std::string {
    return i < state.snapshots.size() ? state.snapshots[i].name : std::string();
  });
  t.fontSize = 13.0f;
  t.wrap = TextWrap::NoWrap;
  t.color = kColorTextDark;
  b.addChild(std::move(t));
  return b;
}

