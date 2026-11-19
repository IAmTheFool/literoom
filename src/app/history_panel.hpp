// app/history_panel.hpp: the History panel
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- History panel ----------------
// Row r shows step (n - 1 - r): newest at the top, the untouched picture at
// the bottom. There are kMaxUndoSteps + 1 rows, shown only for steps that
// exist (the view tree is built once, like the mask list).
static size_t historyStepForRow(size_t r) {
  const size_t n = state.adjust.historySize();
  return r < n ? n - 1 - r : n; // n = "no such step"
}

static void jumpToHistory(size_t step) {
  if (state.dragging || step >= state.adjust.historySize() ||
      step == state.adjust.historyIndex())
    return;
  state.adjust.goToHistory(step);
  uiState.dropCurveSelections();
  updateImageFromAdjustments(); // same follow-up as undo() / redo()
}

static View buildHistoryRow(size_t r) {
  View b;
  b.style.width = Size::full();
  b.style.height = Size::pixel(26);
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.alignItems = Align::Center;
  b.style.padding = EdgeInsets{0, 10, 0, 10};
  b.style.overflowX = Overflow::Hidden;
  b.style.hoverColor = kColorActivityHover;
  b.style.display = std::function<Display()>([r] {
    return state.activeActivity == kHistoryActivity &&
                   r < state.adjust.historySize()
               ? Display::Flex
               : Display::None;
  });
  const auto isCur = [r] {
    return historyStepForRow(r) == state.adjust.historyIndex();
  };
  b.style.backgroundColor = std::function<Color()>([isCur]() -> Color {
    return isCur() ? kColorDropdownRowHover : kColorActivityBarBg;
  });
  b.style.borderWidth =
      std::function<float()>([isCur] { return isCur() ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  b.onClick = [r] { jumpToHistory(historyStepForRow(r)); };

  Text t;
  t.label = std::function<std::string()>([r]() -> std::string {
    // Skip the diffing entirely while the pane is closed.
    if (state.activeActivity != kHistoryActivity)
      return std::string();
    const size_t s = historyStepForRow(r);
    return s < state.adjust.historySize() ? state.adjust.historyLabel(s)
                                          : std::string();
  });
  t.fontSize = 13.0f;
  t.wrap = TextWrap::NoWrap;
  // Steps ahead of the current one (what Redo would bring back) are dimmed.
  t.color = std::function<Color()>([r]() -> Color {
    return historyStepForRow(r) > state.adjust.historyIndex() ? kColorTextMuted
                                                              : kColorTextDark;
  });
  b.addChild(std::move(t));
  return b;
}

// One row of the selected mask's PART list: "1   Radial", "2   Subtract:
// Brush". Shown only while the mask has more than one part (a one-part mask
// looks exactly as it always did).
static View buildPartRow(size_t j) {
  View b;
  b.style.width = Size::full();
  b.style.height = Size::pixel(26);
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.alignItems = Align::Center;
  b.style.padding = EdgeInsets{0, 10, 0, 18};
  b.style.hoverColor = kColorActivityHover;
  b.style.display = std::function<Display()>([j] {
    const Mask *m = selectedMask();
    return m && m->parts.size() > 1 && j < m->parts.size() ? Display::Flex
                                                           : Display::None;
  });
  const auto isSel = [j] {
    return selMask() >= 0 && static_cast<size_t>(selPart()) == j;
  };
  b.style.backgroundColor = std::function<Color()>([isSel]() -> Color {
    return isSel() ? kColorDropdownRowHover : kColorActivityBarBg;
  });
  b.style.borderWidth =
      std::function<float()>([isSel] { return isSel() ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  b.onClick = [j] {
    state.mask.part = static_cast<int>(j);
    state.markDirty(); // move the handles to it
  };

  Text t;
  t.label = std::function<std::string()>([j]() -> std::string {
    const Mask *m = selectedMask();
    if (!m || j >= m->parts.size())
      return std::string();
    std::string s = std::to_string(j + 1) + "   ";
    if (j > 0)
      s += std::string(maskOpName(m->parts[j].op)) + ": ";
    s += maskTypeName(m->parts[j].type());
    return s;
  });
  t.fontSize = 12.0f;
  t.color = kColorTextDark;
  b.addChild(std::move(t));
  return b;
}

// A shape slider for the selected part. `get` / `set` see the shape
// itself; `control` names the undo-coalescing control.
template <class Shape>
static SliderSpec maskShapeSlider(const char *name, float lo, float hi,
                                  std::function<float(const Shape &)> get,
                                  std::function<void(Shape &, float)> set,
                                  int control) {
  SliderSpec s;
  s.name = name;
  s.lo = lo;
  s.hi = hi;
  s.trackW = kMaskTrackW;
  // The sliders show and edit the shape as it sits in the picture on screen.
  s.get = [get] {
    const MaskPart *p = selectedPart();
    const auto r =
        p ? shapeInView<Shape>(*p, state.adjust.geoFrame()) : std::nullopt;
    return r ? get(*r) : 0.0f;
  };
  s.set = [set, control](float v) {
    const GeoFrame frame = state.adjust.geoFrame();
    const size_t part = static_cast<size_t>(selPart());
    maskEdit(
        selMask(),
        [&](Mask &m) {
          editShapeInView<Shape>(m, part, frame, [&](Shape &r) { set(r, v); });
        },
        control);
  };
  s.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v)));
  };
  s.onEnd = [] { state.adjust.endMaskEdit(); };
  return s;
}
static SliderSpec
maskRadialSlider(const char *name, float lo, float hi,
                 std::function<float(const RadialShape &)> get,
                 std::function<void(RadialShape &, float)> set, int control) {
  return maskShapeSlider<RadialShape>(name, lo, hi, std::move(get),
                                      std::move(set), control);
}
static SliderSpec
maskLinearSlider(const char *name, float lo, float hi,
                 std::function<float(const LinearShape &)> get,
                 std::function<void(LinearShape &, float)> set, int control) {
  return maskShapeSlider<LinearShape>(name, lo, hi, std::move(get),
                                      std::move(set), control);
}

// The mask's own copy of one Adjust slider (Exposure, Contrast, ...): one per
// kAdjustmentSpecs entry, so a new global adjustment appears here too.
static View buildMaskAdjustmentSlider(const AdjustmentSpec &spec) {
  const AdjId id = spec.id;
  SliderSpec s;
  s.name = spec.label;
  s.lo = spec.min;
  s.hi = spec.max;
  s.trackW = kMaskTrackW;
  s.get = [id] {
    const Mask *m = selectedMask();
    return m ? m->adj[static_cast<size_t>(id)] : 0.0f;
  };
  s.set = [id](float v) {
    maskEdit(
        selMask(),
        [&](Mask &m) { m.adj[static_cast<size_t>(id)] = std::round(v); },
        kMaskCtlAdj + static_cast<int>(id));
  };
  s.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v)));
  };
  s.onEnd = [] { state.adjust.endMaskEdit(); };
  return buildSliderRow(s);
}

