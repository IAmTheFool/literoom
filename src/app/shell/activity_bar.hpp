// app/shell/activity_bar.hpp: the activity bar (buildActivityBar)
#pragma once

#include <functional> // std::function
#include <utility>    // std::move
#include <vector>     // std::vector

#include "liteui.hpp"            // View, Text, Size, Align, Justify, FlexDirection, EdgeInsets,
                                 // FontWeight, Color

#include "app/color_ui.hpp"      // kGradePaneW
#include "app/constants.hpp"     // Activity, kActivityBarW, kNoActivity, kColorGradingActivity,
                                 // kColorActivityBarBg, kColorActivityHover,
                                 // kColorActivityActive, kColorTextIcon
#include "app/editor_state.hpp"  // inline EditorState state

// ---- Activity bar ----
inline View buildActivityBar(const std::vector<Activity> &activities) {
  View activityBar;
  activityBar.style.width = Size::pixel(kActivityBarW);
  activityBar.style.height = Size::full();
  activityBar.style.flexShrink = 0;
  activityBar.style.direction = FlexDirection::Column;
  activityBar.style.alignItems = Align::Center;
  activityBar.style.backgroundColor = kColorActivityBarBg;
  activityBar.style.padding = EdgeInsets::all(6.0f);
  activityBar.style.gap = 10.0f;

  for (const auto &item : activities) {
    View button;
    button.style.width = Size::pixel(32);
    button.style.height = Size::pixel(32);
    button.style.borderRadius = 6.0f;
    button.style.justifyContent = Justify::Center;
    button.style.alignItems = Align::Center;
    button.style.hoverColor = kColorActivityHover;

    int id = item.id;
    button.style.backgroundColor = std::function<Color()>([id] {
      return state.activeActivity == id ? kColorActivityActive
                                        : kColorActivityBarBg;
    });
    button.onClick = [id] {
      state.activeActivity = (state.activeActivity == id) ? kNoActivity : id;
      // The wheels are laid out for a minimum sidebar width.
      if (state.activeActivity == kColorGradingActivity &&
          state.sidebarWidth < kGradePaneW)
        state.sidebarWidth = kGradePaneW;
      state.markDirty(); // show/hide the crop overlay
    };

    Text icon;
    icon.label = item.icon;
    icon.fontSize = 15.0f;
    icon.fontWeight = FontWeight::SemiBold;
    icon.color = kColorTextIcon;
    button.addChild(std::move(icon));

    activityBar.addChild(std::move(button));
  }

  return activityBar;
}
