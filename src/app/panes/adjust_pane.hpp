// app/panes/adjust_pane.hpp: the Adjust pane (buildAdjustPane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "Adjust" pane: one slider row per entry in kAdjustmentSpecs. Adding
// a new adjustment later is a one-line addition to that table — this
// loop, the render pass (renderAdjusted), and buildAdjustmentSlider all need
// no changes.
static View buildAdjustPane() {
  View adjustPane;
  adjustPane.style.width = Size::full();
  adjustPane.style.direction = FlexDirection::Column;
  adjustPane.style.gap = 16.0f;
  adjustPane.style.display = std::function<Display()>(
      [] { return state.activeActivity == 1 ? Display::Flex : Display::None; });

  bool colorHeaderAdded = false, filtersHeaderAdded = false,
       detailHeaderAdded = false, effectsHeaderAdded = false;
  adjustPane.addChild(sectionLabel("Tone"));
  adjustPane.addChild(sidebarButton(
      "Auto Tone (A)", [] { autoTone(); }, {}, false,
      [] { return state.source != nullptr && !state.dragging; }));
  for (const auto &spec : kAdjustmentSpecs) {
    // LUT Amount is the LUTs pane's slider (it only scales the chosen LUT).
    if (spec.id == AdjId::LutAmount)
      continue;
    if (!colorHeaderAdded && spec.stage == AdjustmentSpec::Stage::Color) {
      adjustPane.addChild(sectionLabel("Color"));
      colorHeaderAdded = true;
    }
    // Filters are Color-stage entries; AdjId::Greyscale starts their block.
    if (!filtersHeaderAdded && spec.id == AdjId::Greyscale) {
      adjustPane.addChild(sectionLabel("Filters"));
      filtersHeaderAdded = true;
    }
    if (!detailHeaderAdded && spec.stage == AdjustmentSpec::Stage::Detail) {
      adjustPane.addChild(sectionLabel("Detail"));
      detailHeaderAdded = true;
    }
    // Vignette and Grain (Stage::Effects, the end of the table): the very last
    // step of the render, after the masks.
    if (!effectsHeaderAdded && spec.stage == AdjustmentSpec::Stage::Effects) {
      adjustPane.addChild(sectionLabel("Effects"));
      effectsHeaderAdded = true;
    }
    // The white-balance picker sits at the top of the Color block, above
    // Temperature, the first slider it sets.
    if (spec.id == AdjId::Temperature)
      adjustPane.addChild(sidebarButton(
          "Pick White Balance (W)", [] { toggleWbPicker(); },
          [] { return wbPickerActive(); }, false,
          [] { return state.image != nullptr && !state.dragging; }));
    adjustPane.addChild(buildAdjustmentSlider(spec));
  }

  return adjustPane;
}
