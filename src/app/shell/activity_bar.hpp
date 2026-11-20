// app/shell/activity_bar.hpp: the activity bar (buildActivityBar)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// ---- Activity bar ----
static View buildActivityBar(const std::vector<Activity> &activities) {
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
