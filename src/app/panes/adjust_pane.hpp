// app/panes/adjust_pane.hpp: the Adjust pane (buildAdjustPane)
#pragma once

// Tabs of the Adjust pane.
enum class AdjustTab : int { Tone, Color, Filters, Detail, Effects, Count };

inline constexpr const char *kAdjustTabNames[] = {"Tone", "Color", "Filters",
                                                  "Detail", "Effects"};
inline AdjustTab gAdjustTab = AdjustTab::Tone; // remembered while the app runs

// Which tab a spec belongs to. Filters are Color-stage entries that start at
// AdjId::Greyscale, so the caller tells us whether that point was reached.
inline AdjustTab adjustTabFor(const AdjustmentSpec &spec, bool pastGreyscale) {
  switch (spec.stage) {
  case AdjustmentSpec::Stage::Effects:
    return AdjustTab::Effects;
  case AdjustmentSpec::Stage::Detail:
    return AdjustTab::Detail;
  case AdjustmentSpec::Stage::Color:
    return pastGreyscale ? AdjustTab::Filters : AdjustTab::Color;
  default:
    return AdjustTab::Tone;
  }
}

inline View buildAdjustTabBar() {
  View bar;
  bar.style.width = Size::full();
  bar.style.direction = FlexDirection::Row;
  bar.style.gap = 2.0f;

  for (int i = 0; i < static_cast<int>(AdjustTab::Count); ++i) {
    const AdjustTab tab = static_cast<AdjustTab>(i);

    View btn;
    btn.style.flexGrow = 1;
    btn.style.height = Size::pixel(24);
    btn.style.borderRadius = 4.0f;
    btn.style.justifyContent = Justify::Center;
    btn.style.alignItems = Align::Center;
    btn.style.hoverColor = kColorActivityHover;
    btn.style.backgroundColor = std::function<Color()>([tab] {
      return gAdjustTab == tab ? kColorActivityActive : kColorTransparent;
    });
    btn.onClick = [tab] { gAdjustTab = tab; };

    Text t;
    t.label = std::string(kAdjustTabNames[i]);
    t.fontSize = 11.0f;
    t.wrap = TextWrap::NoWrap;
    t.color = std::function<Color()>([tab] {
      return gAdjustTab == tab ? kColorTextDark : kColorTextMuted;
    });
    btn.addChild(std::move(t));

    bar.addChild(std::move(btn));
  }
  return bar;
}

// -- "Adjust" pane: a tab bar, then one slider column per tab. Adding a new
// adjustment later is still a one-line addition to kAdjustmentSpecs; it lands
// in the tab that adjustTabFor() picks.
inline View buildAdjustPane() {
  View adjustPane;
  adjustPane.style.width = Size::full();
  adjustPane.style.direction = FlexDirection::Column;
  adjustPane.style.gap = 12.0f;
  adjustPane.style.display = std::function<Display()>([] {
    return state.activeActivity == kAdjustActivity ? Display::Flex
                                                   : Display::None;
  });

  adjustPane.addChild(buildAdjustTabBar());

  // One column per tab, each shown only while its tab is selected.
  std::array<View, static_cast<size_t>(AdjustTab::Count)> pages;
  for (size_t i = 0; i < pages.size(); ++i) {
    const AdjustTab tab = static_cast<AdjustTab>(i);
    pages[i].style.width = Size::full();
    pages[i].style.direction = FlexDirection::Column;
    pages[i].style.gap = 16.0f;
    pages[i].style.display = std::function<Display()>([tab] {
      return gAdjustTab == tab ? Display::Flex : Display::None;
    });
  }
  auto page = [&](AdjustTab t) -> View & {
    return pages[static_cast<size_t>(t)];
  };

  page(AdjustTab::Tone).addChild(sidebarButton(
      "Auto Tone (A)", [] { autoTone(); }, {}, false,
      [] { return state.source != nullptr && !state.dragging; }));

  bool pastGreyscale = false;
  for (const auto &spec : kAdjustmentSpecs) {
    // LUT Amount is the LUTs pane's slider (it only scales the chosen LUT).
    if (spec.id == AdjId::LutAmount)
      continue;
    if (spec.id == AdjId::Greyscale)
      pastGreyscale = true;

    View &dst = page(adjustTabFor(spec, pastGreyscale));

    // The white-balance picker sits above Temperature, the first slider it sets.
    if (spec.id == AdjId::Temperature)
      dst.addChild(sidebarButton(
          "Pick White Balance (W)", [] { toggleWbPicker(); },
          [] { return wbPickerActive(); }, false,
          [] { return state.image != nullptr && !state.dragging; }));
    dst.addChild(buildAdjustmentSlider(spec));
  }

  for (auto &p : pages)
    adjustPane.addChild(std::move(p));

  return adjustPane;
}