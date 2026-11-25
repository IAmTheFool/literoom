// app/panes/mix_pane.hpp: the Color Mix pane (buildMixPane)
#pragma once

#include <cstddef>    // size_t
#include <functional> // std::function
#include <string>     // std::string
#include <utility>    // std::move

#include "liteui.hpp" // View, Text, Size, FlexDirection, Justify, Align,
                      // FontWeight, Display, Color

#include "app/color_ui.hpp"  // buildMixSlider, resetColorMix
#include "app/constants.hpp" // kColorMixActivity, kMixTabW, kColorActivityHover,
                             // kColorDropdownRowHover, kColorActivityBarBg,
                             // kColorDividerHover, kColorTextDark
#include "app/editor_state.hpp" // inline EditorState state
#include "app/ui_state.hpp"     // uiState (.mix)
#include "app/widgets.hpp"      // sidebarButton
#include "color/color_mix.hpp"  // MixChan, kMixChanCount, kMixBandCount,
                                // mixChanAnyNonZero

// -- "Color Mix" pane: HSL mixer. Eight color bands, each with Hue,
// Saturation and Luminance; the tabs pick which of the three sets of eight
// sliders is shown (a bullet marks a tab that has been changed, so an edit
// on a tab you've since switched away from isn't forgotten).
inline View buildMixPane() {
  View mixPane;
  mixPane.style.width = Size::full();
  mixPane.style.direction = FlexDirection::Column;
  mixPane.style.gap = 12.0f;
  mixPane.style.display = std::function<Display()>([] {
    return state.activeActivity == kColorMixActivity ? Display::Flex
                                                     : Display::None;
  });

  View mixTabs;
  mixTabs.style.direction = FlexDirection::Row;
  mixTabs.style.gap = 6.0f;
  mixTabs.style.width = Size::pixel(3.0f * kMixTabW + 2.0f * 6.0f);
  mixTabs.style.flexShrink = 0;
  for (int c = 0; c < static_cast<int>(kMixChanCount); ++c) {
    const auto chan = static_cast<MixChan>(c);
    View tab;
    tab.style.width = Size::pixel(kMixTabW);
    tab.style.height = Size::pixel(26);
    tab.style.flexShrink = 0;
    tab.style.borderRadius = 4.0f;
    tab.style.justifyContent = Justify::Center;
    tab.style.alignItems = Align::Center;
    tab.style.hoverColor = kColorActivityHover;
    tab.style.backgroundColor = std::function<Color()>([chan]() -> Color {
      return uiState.mix.chan == chan ? kColorDropdownRowHover
                                      : kColorActivityBarBg;
    });
    tab.style.borderWidth = std::function<float()>(
        [chan] { return uiState.mix.chan == chan ? 1.0f : 0.0f; });
    tab.style.borderColor = kColorDividerHover;
    tab.onClick = [chan] { uiState.mix.chan = chan; };

    Text tabText;
    tabText.label = std::function<std::string()>([chan]() -> std::string {
      std::string name = chan == MixChan::Hue          ? "Hue"
                         : chan == MixChan::Saturation ? "Sat"
                                                       : "Lum";
      if (mixChanAnyNonZero(state.adjust.mixValues(), chan))
        name += " \u2022";
      return name;
    });
    tabText.fontSize = 12.0f;
    tabText.fontWeight = FontWeight::SemiBold;
    tabText.color = kColorTextDark;
    tab.addChild(std::move(tabText));
    mixTabs.addChild(std::move(tab));
  }
  mixPane.addChild(std::move(mixTabs));

  // One group of eight sliders per tab; only the selected tab's is displayed.
  for (int c = 0; c < static_cast<int>(kMixChanCount); ++c) {
    const auto chan = static_cast<MixChan>(c);
    View group;
    group.style.width = Size::full();
    group.style.direction = FlexDirection::Column;
    group.style.gap = 12.0f;
    group.style.display = std::function<Display()>([chan] {
      return uiState.mix.chan == chan ? Display::Flex : Display::None;
    });
    for (size_t band = 0; band < kMixBandCount; ++band)
      group.addChild(buildMixSlider(chan, band));
    mixPane.addChild(std::move(group));
  }

  mixPane.addChild(sidebarButton("Reset Color Mix", resetColorMix));

  return mixPane;
}
