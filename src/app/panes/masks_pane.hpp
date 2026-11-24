// app/panes/masks_pane.hpp: the Masks pane and its right-hand editor (buildMaskPanes)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// The Masks pane (goes in the sidebar) and the editor for the selected mask
// (goes in the right-hand panel, built later: see addMaskPanel).
struct MaskPaneViews {
  View pane;
  View editor;
};

// -- "Masks" pane: local adjustments. Each mask is a shape plus its own copy
// of the Adjust sliders, applied only where the shape is (see the "masks"
// block). Pick or add a mask in the list; the selected one is drawn on the
// image, where it can be dragged (inside = move, square handles = resize).
inline MaskPaneViews buildMaskPanes() {
  View maskPane;
  maskPane.style.width = Size::full();
  maskPane.style.direction = FlexDirection::Column;
  maskPane.style.gap = 6.0f;
  maskPane.style.display = std::function<Display()>(
      [] { return state.activeActivity == kMasksActivity ? Display::Flex : Display::None; });

  Text maskHint;
  maskHint.label = std::function<std::string()>([]() -> std::string {
    return state.image
               ? "Add a mask, then adjust it in the panel on the right. "
                 "Drag the shape on the image to move it, or its square "
                 "handles to resize (on a linear gradient they also turn "
                 "it). With a brush, drag on the image to paint. With a "
                 "polygon, click to place points (right-click a point "
                 "to remove it) or switch on Freehand lasso and drag. "
                 "Pick another mask from the list."
               : "Open an image to add masks.";
  });
  maskHint.fontSize = 12.0f;
  maskHint.color = kColorTextMuted;
  maskHint.style.margin = EdgeInsets{0, 0, 6, 0};
  maskPane.addChild(std::move(maskHint));

  // One button per kind of mask.
  maskPane.addChild(sidebarButton(
      "+ Radial Gradient", [] { addMask(MaskType::Radial); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Linear Gradient", [] { addMask(MaskType::Linear); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Rectangle", [] { addMask(MaskType::Rectangle); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Polygon / Lasso", [] { addMask(MaskType::Polygon); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Brush", [] { addMask(MaskType::Brush); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Luminosity Range", [] { addMask(MaskType::Luminosity); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Object", [] { addMask(MaskType::Object); }, {}, true,
      [] { return state.original != nullptr; }));
  maskPane.addChild(sidebarButton(
      "+ Color Range", [] { addMask(MaskType::ColorRange); }, {}, true,
      [] { return state.original != nullptr; }));

  for (size_t i = 0; i < kMaxMasks; ++i)
    maskPane.addChild(buildMaskRow(i));

  // Everything below applies to the selected mask and is hidden without one.
  // It is shown in the RIGHT-HAND panel (built after the canvas, see
  // maskPanel below): On / Invert / Tint chips, the brush settings, then
  // three tabs: Effect (the sliders), Shape and Parts.
  View maskEditor;
  maskEditor.style.width = Size::full();
  maskEditor.style.direction = FlexDirection::Column;
  maskEditor.style.gap = 12.0f;
  maskEditor.style.display = std::function<Display()>(
      [] { return selMask() >= 0 ? Display::Flex : Display::None; });

  // Equal-width chip / tab: `n` of them share a row of the panel.
  const auto maskChipFn = [](std::function<std::string()> label, int n,
                             std::function<void()> onClick,
                             std::function<bool()> selected) {
    View c;
    c.style.width =
        std::function<Size()>([n] { return Size::pixel(maskChipW(n)); });
    c.style.height = Size::pixel(26);
    c.style.flexShrink = 0;
    c.style.borderRadius = 4.0f;
    c.style.justifyContent = Justify::Center;
    c.style.alignItems = Align::Center;
    c.style.hoverColor = kColorActivityHover;
    c.style.backgroundColor = std::function<Color()>([selected]() -> Color {
      return selected() ? kColorDropdownRowHover : kColorActivityBarBg;
    });
    c.style.borderWidth =
        std::function<float()>([selected] { return selected() ? 1.0f : 0.0f; });
    c.style.borderColor = kColorDividerHover;
    c.onClick = std::move(onClick);
    Text t;
    t.label = std::move(label);
    t.fontSize = 12.0f;
    t.fontWeight = FontWeight::SemiBold;
    t.color = kColorTextDark;
    c.addChild(std::move(t));
    return c;
  };

  // Same chip with a fixed label.
  const auto maskChip = [maskChipFn](const char *label, int n,
                                     std::function<void()> onClick,
                                     std::function<bool()> selected) {
    const std::string text = label;
    return maskChipFn([text] { return text; }, n, std::move(onClick),
                      std::move(selected));
  };

  View maskFlags;
  maskFlags.style.direction = FlexDirection::Row;
  maskFlags.style.gap = 4.0f;
  maskFlags.style.flexShrink = 0;
  maskFlags.addChild(maskChip(
      "On", 3,
      [] { maskEditDiscrete([](Mask &m) { m.enabled = !m.enabled; }); },
      [] {
        const Mask *m = selectedMask();
        return m && m->enabled;
      }));
  maskFlags.addChild(maskChip(
      "Invert", 3,
      [] { maskEditDiscrete([](Mask &m) { m.invert = !m.invert; }); },
      [] {
        const Mask *m = selectedMask();
        return m && m->invert;
      }));
  maskFlags.addChild(maskChip(
      "Tint", 3,
      [] {
        bool &flag = tintFlag();
        flag = !flag;
        state.markDirty();
      },
      [] { return tintFlag(); }));

  // Name field. TextInput is Canvas-backed and wires up the caret, click to
  // place it, arrows, Backspace / Delete, Ctrl+V and Enter itself.
  View maskNameRow;
  maskNameRow.style.width = Size::full();
  maskNameRow.style.flexShrink = 0;
  maskNameRow.style.display = std::function<Display()>([]() -> Display {
    syncMaskNameField(); // polled every cycle: follows selection and undo
    return Display::Flex;
  });
  {
    TextInput ti;
    ti.placeholder = "Name this mask";
    ti.fontSize = 13.0f;
    ti.textColor = kColorTextDark;
    ti.placeholderColor = kColorTextMuted;
    ti.caretColor = kColorTextDark;
    ti.borderColor = kColorDividerHover;
    ti.focusedBorderColor = Color{90, 150, 235, 255};
    ti.style.width = Size::full();
    ti.style.height = Size::pixel(28);
    ti.style.borderWidth = 1.0f;
    ti.style.borderRadius = 4.0f;
    ti.style.backgroundColor = kColorActivityBarBg;
    ti.onChange = [](const std::string &s) { renameSelectedMask(s); };
    ti.onSubmit = [](const std::string &) {
      state.adjust.endMaskEdit();
    }; // Enter closes the undo step
    uiState.names.mask = ti.state;
    maskNameRow.addChild(std::move(ti));
    registerTextField(uiState.names.mask, maskNameRow.children.back());
  }
  maskEditor.addChild(std::move(maskNameRow));

  maskEditor.addChild(std::move(maskFlags));

  // Overlay color: one chip per kMaskTintPalette entry.
  View maskTintRow;
  maskTintRow.style.direction = FlexDirection::Row;
  maskTintRow.style.gap = 4.0f;
  maskTintRow.style.flexShrink = 0;
  for (int k = 0; k < kMaskTintCount; ++k)
    maskTintRow.addChild(maskChip(
        kMaskTintPalette[k].name, kMaskTintCount, [k] { setMaskTintColor(k); },
        [k] { return state.maskTintColor == k; }));
  maskEditor.addChild(std::move(maskTintRow));

  // The three tab bodies. Only the one picked in state.maskTab is shown.
  const auto maskTabView = [](int tab) {
    View v;
    v.style.width = Size::full();
    v.style.direction = FlexDirection::Column;
    v.style.gap = 12.0f;
    v.style.display = std::function<Display()>([tab]() -> Display {
      return state.maskTab == tab ? Display::Flex : Display::None;
    });
    return v;
  };
  View maskTabEffect = maskTabView(0);
  View maskTabShape = maskTabView(1);
  View maskTabParts = maskTabView(2);

  // Parts: combine several shapes into this mask. Each part after the first
  // is joined to the ones before it (Add / Subtract / Intersect); the rows
  // only appear once there is more than one.
  View partsSec;
  partsSec.style.width = Size::full();
  partsSec.style.direction = FlexDirection::Column;
  partsSec.style.gap = 6.0f;
  partsSec.addChild(sectionLabel("Parts"));
  for (size_t j = 0; j < kMaxMaskParts; ++j)
    partsSec.addChild(buildPartRow(j));
  View opRow; // how the selected part joins the rest (not for the first)
  opRow.style.width = Size::full();
  opRow.style.direction = FlexDirection::Column;
  opRow.style.gap = 6.0f;
  opRow.style.display = std::function<Display()>([]() -> Display {
    return selPart() > 0 ? Display::Flex : Display::None;
  });
  for (const MaskOp op : {MaskOp::Add, MaskOp::Subtract, MaskOp::Intersect})
    opRow.addChild(sidebarButton(
        maskOpName(op), [op] { setPartOp(op); },
        [op] {
          const MaskPart *p = selectedPart();
          return p && selPart() > 0 && p->op == op;
        }));
  partsSec.addChild(std::move(opRow));
  partsSec.addChild(
      sidebarButton("+ Part: Radial", [] { addPart(MaskType::Radial); }));
  partsSec.addChild(
      sidebarButton("+ Part: Linear", [] { addPart(MaskType::Linear); }));
  partsSec.addChild(
      sidebarButton("+ Part: Rectangle", [] { addPart(MaskType::Rectangle); }));
  partsSec.addChild(
      sidebarButton("+ Part: Polygon", [] { addPart(MaskType::Polygon); }));
  partsSec.addChild(
      sidebarButton("+ Part: Brush", [] { addPart(MaskType::Brush); }));
  partsSec.addChild(sidebarButton("+ Part: Luminosity",
                                  [] { addPart(MaskType::Luminosity); }));
  partsSec.addChild(
      sidebarButton("+ Part: Object", [] { addPart(MaskType::Object); }));
  partsSec.addChild(sidebarButton("+ Part: Color Range",
                                  [] { addPart(MaskType::ColorRange); }));

  View removeRow;
  removeRow.style.width = Size::full();
  removeRow.style.display = std::function<Display()>([]() -> Display {
    const Mask *m = selectedMask();
    return m && m->parts.size() > 1 ? Display::Flex : Display::None;
  });
  removeRow.addChild(sidebarButton("Remove Part", removeSelectedPart));
  partsSec.addChild(std::move(removeRow));
  maskTabParts.addChild(std::move(partsSec));

  // Shape sliders, per mask type (shown only for that type).
  View radialShape;
  radialShape.style.width = Size::full();
  radialShape.style.direction = FlexDirection::Column;
  radialShape.style.gap = 12.0f;
  radialShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::Radial ? Display::Flex
                                                  : Display::None;
  });
  radialShape.addChild(sectionLabel("Shape"));
  radialShape.addChild(buildSliderRow(maskRadialSlider(
      "Width", kMaskMinRadius * 100.0f, kMaskMaxRadius * 100.0f,
      [](const RadialShape &r) { return r.rx * 100.0f; },
      [](RadialShape &r, float v) { r.rx = v / 100.0f; }, kMaskCtlWidth)));
  radialShape.addChild(buildSliderRow(maskRadialSlider(
      "Height", kMaskMinRadius * 100.0f, kMaskMaxRadius * 100.0f,
      [](const RadialShape &r) { return r.ry * 100.0f; },
      [](RadialShape &r, float v) { r.ry = v / 100.0f; }, kMaskCtlHeight)));
  radialShape.addChild(buildSliderRow(maskRadialSlider(
      "Angle", -90.0f, 90.0f, [](const RadialShape &r) { return r.angle; },
      [](RadialShape &r, float v) { r.angle = v; }, kMaskCtlAngle)));
  radialShape.addChild(buildSliderRow(maskRadialSlider(
      "Feather", 0.0f, 100.0f,
      [](const RadialShape &r) { return r.feather * 100.0f; },
      [](RadialShape &r, float v) { r.feather = v / 100.0f; },
      kMaskCtlFeather)));
  maskTabShape.addChild(std::move(radialShape));

  View rectShape;
  rectShape.style.width = Size::full();
  rectShape.style.direction = FlexDirection::Column;
  rectShape.style.gap = 12.0f;
  rectShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::Rectangle ? Display::Flex
                                                     : Display::None;
  });
  rectShape.addChild(sectionLabel("Shape"));
  rectShape.addChild(buildSliderRow(maskShapeSlider<RectShape>(
      "Width", kMaskMinRadius * 100.0f, kMaskMaxRadius * 100.0f,
      [](const RectShape &r) { return r.rx * 100.0f; },
      [](RectShape &r, float v) { r.rx = v / 100.0f; }, kMaskCtlWidth)));
  rectShape.addChild(buildSliderRow(maskShapeSlider<RectShape>(
      "Height", kMaskMinRadius * 100.0f, kMaskMaxRadius * 100.0f,
      [](const RectShape &r) { return r.ry * 100.0f; },
      [](RectShape &r, float v) { r.ry = v / 100.0f; }, kMaskCtlHeight)));
  rectShape.addChild(buildSliderRow(maskShapeSlider<RectShape>(
      "Angle", -90.0f, 90.0f, [](const RectShape &r) { return r.angle; },
      [](RectShape &r, float v) { r.angle = v; }, kMaskCtlAngle)));
  rectShape.addChild(buildSliderRow(maskShapeSlider<RectShape>(
      "Feather", 0.0f, 100.0f,
      [](const RectShape &r) { return r.feather * 100.0f; },
      [](RectShape &r, float v) { r.feather = v / 100.0f; }, kMaskCtlFeather)));
  maskTabShape.addChild(std::move(rectShape));

  View polyShape;
  polyShape.style.width = Size::full();
  polyShape.style.direction = FlexDirection::Column;
  polyShape.style.gap = 12.0f;
  polyShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::Polygon ? Display::Flex
                                                   : Display::None;
  });
  polyShape.addChild(sectionLabel("Polygon"));
  {
    Text t;
    t.label = std::function<std::string()>([]() -> std::string {
      const MaskPart *p = selectedPart();
      const auto *s = p ? std::get_if<PolygonShape>(&p->shape) : nullptr;
      const size_t n = s ? s->pts.size() : 0;
      if (state.mask.polyLasso)
        return "Drag on the picture to draw an outline (it replaces the "
               "current one)";
      if (n == 0)
        return "Click the picture to place the first point";
      if (n < 3)
        return std::to_string(n) +
               " point(s): add at least 3 to select an area";
      return std::to_string(n) +
             " points. Click outside to add one, drag a point or side, "
             "drag inside to move, right-click a point to remove it";
    });
    t.fontSize = 12.0f;
    t.color = kColorTextMuted;
    polyShape.addChild(std::move(t));
  }
  polyShape.addChild(buildSliderRow(maskShapeSlider<PolygonShape>(
      "Feather", 0.0f, 100.0f,
      [](const PolygonShape &s) { return s.feather * 100.0f; },
      [](PolygonShape &s, float v) { s.feather = v / 100.0f; },
      kMaskCtlFeather)));
  polyShape.addChild(sidebarButton(
      "Freehand lasso",
      [] {
        state.mask.polyLasso = !state.mask.polyLasso;
        state.markDirty();
      },
      [] { return state.mask.polyLasso; }));
  polyShape.addChild(sidebarButton("Clear points", [] {
    const size_t part = static_cast<size_t>(selPart());
    const GeoFrame frame = state.adjust.geoFrame();
    maskEditDiscrete([&](Mask &m) {
      editShapeInView<PolygonShape>(m, part, frame,
                                    [](PolygonShape &s) { s.pts.clear(); });
    });
  }));
  maskTabShape.addChild(std::move(polyShape));

  View linearShape;
  linearShape.style.width = Size::full();
  linearShape.style.direction = FlexDirection::Column;
  linearShape.style.gap = 12.0f;
  linearShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::Linear ? Display::Flex
                                                  : Display::None;
  });
  linearShape.addChild(sectionLabel("Shape"));
  linearShape.addChild(buildSliderRow(maskLinearSlider(
      "Length", kMaskMinRadius * 100.0f, kMaskMaxRadius * 100.0f,
      [](const LinearShape &l) { return l.len * 100.0f; },
      [](LinearShape &l, float v) { l.len = v / 100.0f; }, kMaskCtlLength)));
  linearShape.addChild(buildSliderRow(maskLinearSlider(
      "Angle", -180.0f, 180.0f, [](const LinearShape &l) { return l.angle; },
      [](LinearShape &l, float v) { l.angle = v; }, kMaskCtlAngle)));
  maskTabShape.addChild(std::move(linearShape));

  // Luminosity range (shown for a luminosity mask): which brightnesses it
  // selects. Min can never pass Max, nor Max go below Min.
  View lumShape;
  lumShape.style.width = Size::full();
  lumShape.style.direction = FlexDirection::Column;
  lumShape.style.gap = 12.0f;
  lumShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::Luminosity ? Display::Flex
                                                      : Display::None;
  });
  lumShape.addChild(sectionLabel("Range"));
  lumShape.addChild(buildSliderRow(maskShapeSlider<LuminosityShape>(
      "Min", 0.0f, 100.0f,
      [](const LuminosityShape &l) { return l.lo * 100.0f; },
      [](LuminosityShape &l, float v) { l.setLo(v / 100.0f); },
      kMaskCtlLumLo)));
  lumShape.addChild(buildSliderRow(maskShapeSlider<LuminosityShape>(
      "Max", 0.0f, 100.0f,
      [](const LuminosityShape &l) { return l.hi * 100.0f; },
      [](LuminosityShape &l, float v) { l.setHi(v / 100.0f); },
      kMaskCtlLumHi)));
  lumShape.addChild(buildSliderRow(maskShapeSlider<LuminosityShape>(
      "Smoothness", 0.0f, 100.0f,
      [](const LuminosityShape &l) { return l.feather * 100.0f; },
      [](LuminosityShape &l, float v) { l.feather = v / 100.0f; },
      kMaskCtlFeather)));
  maskTabShape.addChild(std::move(lumShape));

  View crShape;
  crShape.style.width = Size::full();
  crShape.style.direction = FlexDirection::Column;
  crShape.style.gap = 12.0f;
  crShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::ColorRange ? Display::Flex
                                                      : Display::None;
  });
  crShape.addChild(sectionLabel("Color Range"));
  {
    View hint;
    Text t;
    t.label = std::function<std::string()>([]() -> std::string {
      const MaskPart *p = selectedPart();
      const auto *s = p ? std::get_if<ColorRangeShape>(&p->shape) : nullptr;
      return s && s->set ? "Click the picture to pick another color"
                         : "Click the picture to pick a color";
    });
    t.fontSize = 12.0f;
    t.color = kColorTextMuted;
    crShape.addChild(std::move(t));
  }
  crShape.addChild(buildSliderRow(maskShapeSlider<ColorRangeShape>(
      "Range", 0.0f, 100.0f,
      [](const ColorRangeShape &s) { return s.range * 100.0f; },
      [](ColorRangeShape &s, float v) { s.range = v / 100.0f; },
      kMaskCtlRange)));
  crShape.addChild(buildSliderRow(maskShapeSlider<ColorRangeShape>(
      "Smoothness", 0.0f, 100.0f,
      [](const ColorRangeShape &s) { return s.feather * 100.0f; },
      [](ColorRangeShape &s, float v) { s.feather = v / 100.0f; },
      kMaskCtlFeather)));
  maskTabShape.addChild(std::move(crShape));

  // Brush settings (shown for a brush mask). They apply to the NEXT stroke;
  // strokes already painted keep the values they were painted with.
  const auto brushSlider = [](const char *name, float lo, float hi,
                              std::function<float()> get,
                              std::function<void(float)> set) {
    SliderSpec s;
    s.name = name;
    s.lo = lo;
    s.hi = hi;
    s.get = std::move(get);
    s.set = std::move(set);
    s.format = [](float v) {
      return std::to_string(static_cast<int>(std::lround(v)));
    };
    s.affectsPixels = false;
    s.trackW = kMaskTrackW;
    return buildSliderRow(s);
  };
  View brushShape;
  brushShape.style.width = Size::full();
  brushShape.style.direction = FlexDirection::Column;
  brushShape.style.gap = 12.0f;
  brushShape.style.display = std::function<Display()>([]() -> Display {
    return selectedIsBrush() ? Display::Flex : Display::None;
  });
  brushShape.addChild(sidebarButton(
      "Erase",
      [] {
        state.mask.brushErase = !state.mask.brushErase;
        state.markDirty();
      },
      [] { return state.mask.brushErase; }));
  brushShape.addChild(brushSlider(
      "Size", 1.0f, kBrushMaxSize * 100.0f,
      [] { return state.mask.brushSize * 100.0f; },
      [](float v) {
        state.mask.brushSize =
            std::clamp(v / 100.0f, kBrushMinSize, kBrushMaxSize);
      }));
  brushShape.addChild(brushSlider(
      "Feather", 0.0f, 100.0f, [] { return state.mask.brushFeather * 100.0f; },
      [](float v) {
        state.mask.brushFeather = std::clamp(v / 100.0f, 0.0f, 1.0f);
      }));
  brushShape.addChild(brushSlider(
      "Density", 1.0f, 100.0f, [] { return state.mask.brushDensity * 100.0f; },
      [](float v) {
        state.mask.brushDensity = std::clamp(v / 100.0f, 0.01f, 1.0f);
      }));
  brushShape.addChild(brushSlider(
      "Flow", 1.0f, 100.0f, [] { return state.mask.brushFlow * 100.0f; },
      [](float v) {
        state.mask.brushFlow = std::clamp(v / 100.0f, 0.01f, 1.0f);
      }));
  brushShape.addChild(sidebarButton("Clear Strokes", [] {
    const size_t part = static_cast<size_t>(selPart());
    maskEditDiscrete([part](Mask &m) {
      if (part < m.parts.size())
        if (auto *b = std::get_if<BrushShape>(&m.parts[part].shape))
          b->strokes.clear();
    });
  }));
  maskEditor.addChild(
      std::move(brushShape)); // pinned above the tabs: size/feather
                              // are used while painting

  View objectShape;
  objectShape.style.width = Size::full();
  objectShape.style.direction = FlexDirection::Column;
  objectShape.style.gap = 12.0f;
  objectShape.style.display = std::function<Display()>([]() -> Display {
    return selectedPartType() == MaskType::Object ? Display::Flex
                                                  : Display::None;
  });
  objectShape.addChild(sidebarButton("Set test map (bright areas)", [] {
    if (auto m = makeTestObjectMap())
      setSelectedObjectMap(std::move(m));
  }));
  objectShape.addChild(buildSliderRow(maskShapeSlider<ObjectShape>(
      "Feather", 0.0f, 100.0f,
      [](const ObjectShape &o) { return o.feather * 100.0f; },
      [](ObjectShape &o, float v) { o.feather = v / 100.0f; },
      kMaskCtlFeather)));
  objectShape.addChild(buildSliderRow(maskShapeSlider<ObjectShape>(
      "Grow", -100.0f, 100.0f,
      [](const ObjectShape &o) { return o.grow * 100.0f; },
      [](ObjectShape &o, float v) { o.grow = v / 100.0f; }, kMaskCtlWidth)));
  objectShape.addChild(sidebarButton(
      "Click adds: Background",
      [] {
        state.mask.objBg = !state.mask.objBg;
        state.markDirty();
      },
      [] { return state.mask.objBg; }));
  objectShape.addChild(sidebarButton("Clear selection", clearSelectedObject));
  {
    Text segInfo;
    segInfo.label = std::function<std::string()>([] { return getSegInfo(); });
    segInfo.fontSize = 11.0f;
    segInfo.color = kColorTextFaint;
    objectShape.addChild(std::move(segInfo));
  }
  maskTabShape.addChild(std::move(objectShape));

  // ---- Effect tab ----
  // A second row of chips picks Basic / Mix / Grade / Curve; a bullet marks
  // a section that has been changed for this mask.
  const auto chipRow = [] {
    View r;
    r.style.direction = FlexDirection::Row;
    r.style.gap = 4.0f;
    r.style.flexShrink = 0;
    return r;
  };
  const auto maskFxView = [](int fx) {
    View v;
    v.style.width = Size::full();
    v.style.direction = FlexDirection::Column;
    v.style.gap = 12.0f;
    v.style.display = std::function<Display()>([fx]() -> Display {
      return state.maskFx == fx ? Display::Flex : Display::None;
    });
    return v;
  };
  View fxBasic = maskFxView(0);
  View fxMix = maskFxView(1);
  View fxGrade = maskFxView(2);
  View fxCurve = maskFxView(3);

  // Basic: Amount plus the same table as the Adjust pane.
  fxBasic.addChild(buildMaskAmountSlider());
  bool maskColorHeaderAdded = false, maskFiltersHeaderAdded = false,
       maskDetailHeaderAdded = false;
  fxBasic.addChild(sectionLabel("Tone"));
  for (const auto &spec : kAdjustmentSpecs) {
    // Vignette and Grain are global only (renderWith applies them after the
    // masks), and so is the 3D LUT: a mask gets no sliders for them.
    if (spec.stage == AdjustmentSpec::Stage::Effects ||
        spec.id == AdjId::LutAmount)
      continue;
    if (!maskColorHeaderAdded && spec.stage == AdjustmentSpec::Stage::Color) {
      fxBasic.addChild(sectionLabel("Color"));
      maskColorHeaderAdded = true;
    }
    if (!maskFiltersHeaderAdded && spec.id == AdjId::Greyscale) {
      fxBasic.addChild(sectionLabel("Filters"));
      maskFiltersHeaderAdded = true;
    }
    if (!maskDetailHeaderAdded && spec.stage == AdjustmentSpec::Stage::Detail) {
      fxBasic.addChild(sectionLabel("Detail"));
      maskDetailHeaderAdded = true;
    }
    fxBasic.addChild(buildMaskAdjustmentSlider(spec));
  }
  fxBasic.addChild(sidebarButton("Reset Basic", [] {
    maskEditDiscrete([](Mask &m) {
      m.adj.fill(0.0f);
      m.amount = 100.0f;
    });
  }));

  // Mix: Hue / Sat / Lum chips, eight band sliders each.
  {
    View chanRow = chipRow();
    for (int c = 0; c < static_cast<int>(kMixChanCount); ++c) {
      const auto chan = static_cast<MixChan>(c);
      chanRow.addChild(maskChipFn(
          [chan]() -> std::string {
            std::string name = chan == MixChan::Hue          ? "Hue"
                               : chan == MixChan::Saturation ? "Sat"
                                                             : "Lum";
            const Mask *m = selectedMask();
            if (m && mixChanAnyNonZero(m->mix, chan))
              name += " \u2022";
            return name;
          },
          3, [chan] { uiState.maskPane.mixChan = chan; },
          [chan] { return uiState.maskPane.mixChan == chan; }));
    }
    fxMix.addChild(std::move(chanRow));
    for (int c = 0; c < static_cast<int>(kMixChanCount); ++c) {
      const auto chan = static_cast<MixChan>(c);
      View group;
      group.style.width = Size::full();
      group.style.direction = FlexDirection::Column;
      group.style.gap = 12.0f;
      group.style.display = std::function<Display()>([chan]() -> Display {
        return uiState.maskPane.mixChan == chan ? Display::Flex : Display::None;
      });
      for (size_t band = 0; band < kMixBandCount; ++band)
        group.addChild(buildMaskMixSlider(chan, band));
      fxMix.addChild(std::move(group));
    }
    fxMix.addChild(sidebarButton("Reset Color Mix", [] {
      maskEditDiscrete([](Mask &m) { m.mix.fill(0.0f); });
    }));
  }

  // Grade: one chip per range, Hue / Saturation / Luminance sliders for the
  // picked range, then Blending and Balance for all of them.
  {
    static const char *const rangeShort[kGradeRangeCount] = {"Shad", "Mid",
                                                             "High", "Glob"};
    View rangeRow = chipRow();
    for (int r = 0; r < static_cast<int>(kGradeRangeCount); ++r)
      rangeRow.addChild(maskChipFn(
          [r]() -> std::string {
            std::string name = rangeShort[r];
            const Mask *m = selectedMask();
            if (m && !m->grade.wheel[static_cast<size_t>(r)].neutral())
              name += " \u2022";
            return name;
          },
          4, [r] { uiState.maskPane.gradeRange = r; },
          [r] { return uiState.maskPane.gradeRange == r; }));
    fxGrade.addChild(std::move(rangeRow));
    for (int r = 0; r < static_cast<int>(kGradeRangeCount); ++r) {
      View group;
      group.style.width = Size::full();
      group.style.direction = FlexDirection::Column;
      group.style.gap = 12.0f;
      group.style.display = std::function<Display()>([r]() -> Display {
        return uiState.maskPane.gradeRange == r ? Display::Flex : Display::None;
      });
      group.addChild(buildMaskGradeSlider("Hue", r, &GradeWheel::hue, 0, 0.0f,
                                          359.0f, false));
      group.addChild(buildMaskGradeSlider("Saturation", r, &GradeWheel::sat, 1,
                                          0.0f, 100.0f, false));
      group.addChild(buildMaskGradeSlider("Luminance", r, &GradeWheel::lum, 2,
                                          -100.0f, 100.0f, true));
      fxGrade.addChild(std::move(group));
    }
    fxGrade.addChild(buildMaskGradeShared(false));
    fxGrade.addChild(buildMaskGradeShared(true));
    fxGrade.addChild(sidebarButton("Reset Color Grading", [] {
      maskEditDiscrete([](Mask &m) { m.grade = ColorGrade{}; });
    }));
  }

  // Curve: RGB / R / G / B chips, the plot, a readout and resets.
  {
    View chRow = chipRow();
    for (int c = 0; c < static_cast<int>(kCurveChannelCount); ++c) {
      const auto ch = static_cast<CurveChannel>(c);
      chRow.addChild(maskChipFn(
          [ch]() -> std::string {
            std::string name = ch == CurveChannel::Master  ? "RGB"
                               : ch == CurveChannel::Red   ? "R"
                               : ch == CurveChannel::Green ? "G"
                                                           : "B";
            const Mask *m = selectedMask();
            if (m && !(m->curves[static_cast<size_t>(ch)] == identityCurve()))
              name += " \u2022";
            return name;
          },
          4,
          [ch] {
            uiState.maskPane.curveActive = -1;
            uiState.maskPane.curveCh = ch;
            uiState.maskPane.curveUiDirty = true;
          },
          [ch] { return uiState.maskPane.curveCh == ch; }));
    }
    fxCurve.addChild(std::move(chRow));
    fxCurve.addChild(buildMaskCurveCanvas());

    Text curveInfo;
    curveInfo.label = std::function<std::string()>([]() -> std::string {
      const CurvePoints p = maskCurveCopy();
      const std::string name = curveChannelName(uiState.maskPane.curveCh);
      if (uiState.maskPane.curveActive >= 0 &&
          uiState.maskPane.curveActive < static_cast<int>(p.size()))
        return name + ": In " +
               std::to_string(static_cast<int>(
                   std::lround(p[static_cast<size_t>(uiState.maskPane.curveActive)].x))) +
               "  Out " +
               std::to_string(static_cast<int>(
                   std::lround(p[static_cast<size_t>(uiState.maskPane.curveActive)].y)));
      return name + ": " + std::to_string(p.size()) + " points";
    });
    curveInfo.fontSize = 13.0f;
    curveInfo.color = kColorTextMid;
    fxCurve.addChild(std::move(curveInfo));

    Text curveHint;
    curveHint.label =
        std::string("Click: add   Drag: move   Right-click: delete");
    curveHint.fontSize = 11.0f;
    curveHint.color = kColorTextFaint;
    fxCurve.addChild(std::move(curveHint));

    fxCurve.addChild(sidebarButton("Reset Curve", [] {
      uiState.maskPane.curveActive = -1;
      uiState.maskPane.curveUiDirty = true;
      maskEditDiscrete([](Mask &m) {
        m.curves[static_cast<size_t>(uiState.maskPane.curveCh)] = identityCurve();
      });
    }));
    fxCurve.addChild(sidebarButton("Reset All Curves", [] {
      uiState.maskPane.curveActive = -1;
      uiState.maskPane.curveUiDirty = true;
      maskEditDiscrete([](Mask &m) { m.curves = identityCurves(); });
    }));
  }

  View fxTabs = chipRow();
  {
    static const char *const fxNames[4] = {"Basic", "Mix", "Grade", "Curve"};
    for (int t = 0; t < 4; ++t)
      fxTabs.addChild(maskChipFn(
          [t]() -> std::string {
            std::string name = fxNames[t];
            const Mask *m = selectedMask();
            bool changed = false;
            if (m)
              switch (t) {
              case 0:
                changed = m->amount != 100.0f;
                for (float v : m->adj)
                  changed = changed || v != 0.0f;
                break;
              case 1:
                changed = mixAnyNonZero(m->mix);
                break;
              case 2:
                changed = !(m->grade == ColorGrade{});
                break;
              default:
                changed = !(m->curves == identityCurves());
                break;
              }
            if (changed)
              name += " \u2022";
            return name;
          },
          4, [t] { state.maskFx = t; }, [t] { return state.maskFx == t; }));
  }
  maskTabEffect.addChild(std::move(fxTabs));
  maskTabEffect.addChild(std::move(fxBasic));
  maskTabEffect.addChild(std::move(fxMix));
  maskTabEffect.addChild(std::move(fxGrade));
  maskTabEffect.addChild(std::move(fxCurve));

  // Tab bar, then the tab bodies, then the always-visible Delete.
  View maskTabs;
  maskTabs.style.direction = FlexDirection::Row;
  maskTabs.style.gap = 4.0f;
  maskTabs.style.flexShrink = 0;
  {
    const char *const tabNames[3] = {"Effect", "Shape", "Parts"};
    for (int t = 0; t < 3; ++t)
      maskTabs.addChild(maskChip(
          tabNames[t], 3, [t] { state.maskTab = t; },
          [t] { return state.maskTab == t; }));
  }
  maskEditor.addChild(std::move(maskTabs));
  maskEditor.addChild(std::move(maskTabEffect));
  maskEditor.addChild(std::move(maskTabShape));
  maskEditor.addChild(std::move(maskTabParts));
  // Mask list operations. Order matters: masks apply top to bottom.
  View maskOps1 = chipRow();
  maskOps1.addChild(
      maskChip("Duplicate", 3, duplicateSelectedMask, [] { return false; }));
  maskOps1.addChild(maskChip(
      "Move Up", 3, [] { moveSelectedMask(-1); }, [] { return false; }));
  maskOps1.addChild(maskChip(
      "Move Down", 3, [] { moveSelectedMask(+1); }, [] { return false; }));
  maskEditor.addChild(std::move(maskOps1));
  View maskOps2 = chipRow();
  maskOps2.addChild(
      maskChip("Copy Effects", 2, copyMaskEffects, [] { return false; }));
  maskOps2.addChild(
      maskChip("Paste Effects", 2, pasteMaskEffects, [] { return false; }));
  maskEditor.addChild(std::move(maskOps2));
  maskEditor.addChild(sidebarButton("Delete Mask", deleteSelectedMask));
  // maskEditor is added to the right-hand panel below (after the canvas).

  return {std::move(maskPane), std::move(maskEditor)};
}
