// app/panes/luts_pane.hpp: the LUTs pane (buildLutsPane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "LUTs" pane: 3D color lookup tables (.cube files made for ordinary sRGB
// / Rec.709 pictures). Click one to apply it (one undo step); LUT Amount sets
// its strength. Import copies files into the library folder. Each row
// previews the open picture under that LUT (made on a background worker; see
// pollLutPreviews). The list is re-read each time the pane is opened.
static View buildLutsPane() {
  View lutsPane;
  lutsPane.style.width = Size::full();
  lutsPane.style.direction = FlexDirection::Column;
  lutsPane.style.gap = 6.0f;
  lutsPane.style.display = std::function<Display()>([] {
    static bool wasShown = false;
    const bool shown = state.activeActivity == kLutsActivity;
    if (shown && !wasShown)
      refreshLuts();
    wasShown = shown;
    return shown ? Display::Flex : Display::None;
  });

  Text lutsHint;
  lutsHint.label =
      std::string("A LUT remaps colors. Click one to apply it to this picture; "
                  "it can be undone. LUT Amount "
                  "sets how much of it is used. Presets, Copy / Paste Settings "
                  "and Sync Settings carry the LUT "
                  "too. Made for camera-log footage? It may look wrong here.");
  lutsHint.fontSize = 12.0f;
  lutsHint.color = kColorTextMuted;
  lutsHint.style.margin = EdgeInsets{0, 0, 6, 0};
  lutsPane.addChild(std::move(lutsHint));

  lutsPane.addChild(sidebarButton("Import LUT...", importLuts, {}, true));

  Text lutsCurrent;
  lutsCurrent.label = std::function<std::string()>(
      []() -> std::string { return lutCurrentText(); });
  lutsCurrent.fontSize = 13.0f;
  lutsCurrent.color = kColorTextDark;
  lutsCurrent.style.margin = EdgeInsets{6, 0, 0, 0};
  lutsPane.addChild(std::move(lutsCurrent));

  {
    SliderSpec s;
    s.name = "LUT Amount";
    s.lo = 0.0f;
    s.hi = 100.0f;
    s.get = [] { return state.adjust.get(AdjId::LutAmount); };
    s.set = [](float v) { setLutAmount(v); };
    s.format = [](float v) {
      return std::to_string(static_cast<int>(std::lround(v)));
    };
    View amountBox;
    amountBox.style.width = Size::full();
    amountBox.style.direction = FlexDirection::Column;
    amountBox.style.display = std::function<Display()>([] {
      return state.adjust.lut().empty() ? Display::None : Display::Flex;
    });
    amountBox.addChild(buildSliderRow(s));
    lutsPane.addChild(std::move(amountBox));
  }
  lutsPane.addChild(sidebarButton("Remove LUT", removeLut, {}, false, [] {
    return !state.adjust.lut().empty() && !state.dragging;
  }));

  Text lutsEmpty;
  lutsEmpty.label = std::string("No LUTs yet. Import a .cube file.");
  lutsEmpty.fontSize = 12.0f;
  lutsEmpty.color = kColorTextMuted;
  lutsEmpty.style.margin = EdgeInsets{6, 0, 0, 0};
  lutsEmpty.style.display = std::function<Display()>(
      [] { return gLuts.empty() ? Display::Flex : Display::None; });
  lutsPane.addChild(std::move(lutsEmpty));

  for (size_t i = 0; i < kLutRows; ++i)
    lutsPane.addChild(buildLutRow(i));

  Text lutsFolder;
  lutsFolder.label = "LUT library: " + lutDir();
  lutsFolder.fontSize = 11.0f;
  lutsFolder.color = kColorTextMuted;
  lutsFolder.style.margin = EdgeInsets{8, 0, 0, 0};
  lutsPane.addChild(std::move(lutsFolder));

  return lutsPane;
}
