// app/panes/grade_pane.hpp: the Color Grade pane (buildGradePane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "Color Grade" pane: 3-way / single-range color wheels (Shadows,
// Midtones, Highlights, Global) plus shared Blending and Balance sliders.
static View buildGradePane() {
  View gradePane;
  gradePane.style.width = Size::full();
  gradePane.style.direction = FlexDirection::Column;
  gradePane.style.gap = 10.0f;
  gradePane.style.display = std::function<Display()>(
      [] { return state.activeActivity == kColorGradingActivity ? Display::Flex : Display::None; });

  // Mode tabs: 3-Way | Shadows | Mid | High | Global. A bullet marks a range
  // that has been adjusted.
  View gradeTabs;
  gradeTabs.style.direction = FlexDirection::Row;
  gradeTabs.style.gap = 4.0f;
  gradeTabs.style.flexShrink = 0;
  for (int m = 0; m < 5; ++m) {
    View tab;
    tab.style.width = Size::pixel(kGradeTabW);
    tab.style.height = Size::pixel(26);
    tab.style.flexShrink = 0;
    tab.style.borderRadius = 4.0f;
    tab.style.justifyContent = Justify::Center;
    tab.style.alignItems = Align::Center;
    tab.style.hoverColor = kColorActivityHover;
    tab.style.backgroundColor = std::function<Color()>([m]() -> Color {
      return uiState.grade.mode == m ? kColorDropdownRowHover : kColorActivityBarBg;
    });
    tab.style.borderWidth =
        std::function<float()>([m] { return uiState.grade.mode == m ? 1.0f : 0.0f; });
    tab.style.borderColor = kColorDividerHover;
    tab.onClick = [m] {
      uiState.grade.mode = m;
      if (m > 0)
        uiState.grade.active = m - 1;
    };

    Text tabText;
    tabText.label = std::function<std::string()>([m]() -> std::string {
      static const char *const names[5] = {"3-Way", "Shad", "Mid", "High",
                                           "Glob"};
      std::string name = names[m];
      if (m > 0 && !state.adjust.grade().wheel[m - 1].neutral())
        name += "\u2022";
      return name;
    });
    tabText.fontSize = 11.0f;
    tabText.fontWeight = FontWeight::SemiBold;
    tabText.color = kColorTextDark;
    tab.addChild(std::move(tabText));
    gradeTabs.addChild(std::move(tab));
  }
  gradePane.addChild(std::move(gradeTabs));

  // H:13 S:72 L:+1 for the wheel touched last
  Text gradeReadout;
  gradeReadout.label = std::function<std::string()>([]() -> std::string {
    const int r = uiState.grade.mode > 0 ? uiState.grade.mode - 1 : uiState.grade.active;
    const GradeWheel &w = state.adjust.grade().wheel[r];
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s   H:%d S:%d L:%+d", gradeRangeName(r),
                  static_cast<int>(std::lround(w.hue)),
                  static_cast<int>(std::lround(w.sat)),
                  static_cast<int>(std::lround(w.lum)));
    return buf;
  });
  gradeReadout.fontSize = 12.0f;
  gradeReadout.color = kColorTextMid;
  gradePane.addChild(std::move(gradeReadout));

  // One body per mode; only the selected one is displayed.
  auto modeBody = [](int m, View body) {
    body.style.display = std::function<Display()>(
        [m] { return uiState.grade.mode == m ? Display::Flex : Display::None; });
    return body;
  };
  gradePane.addChild(modeBody(0, buildGradeThreeWay()));
  for (int m = 1; m <= 4; ++m)
    gradePane.addChild(modeBody(m, buildGradeSingle(m - 1)));

  // Blending / Balance, shared by every mode.
  SliderSpec blending;
  blending.name = "Blending";
  blending.lo = 0;
  blending.hi = 100;
  blending.get = [] { return state.adjust.grade().blending; };
  blending.set = [](float v) {
    gradeEdit([&](ColorGrade &g) { g.blending = std::round(v); }, 16);
  };
  blending.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v)));
  };
  blending.onEnd = [] { state.adjust.endGradeEdit(); };
  gradePane.addChild(buildSliderRow(blending));

  SliderSpec balance = blending;
  balance.name = "Balance";
  balance.lo = -100;
  balance.hi = 100;
  balance.get = [] { return state.adjust.grade().balance; };
  balance.set = [](float v) {
    gradeEdit([&](ColorGrade &g) { g.balance = std::round(v); }, 17);
  };
  balance.format = [](float v) {
    const int i = static_cast<int>(std::lround(v));
    return i > 0 ? "+" + std::to_string(i) : std::to_string(i);
  };
  gradePane.addChild(buildSliderRow(balance));

  gradePane.addChild(sidebarButton("Reset Color Grading", resetColorGrade));

  return gradePane;
}
