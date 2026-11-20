// app/shell/sidebar.hpp: the left sidebar: title, histogram and every pane (buildSidebar)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// ---- Sidebar (width driven by activeActivity: 0 when closed) ----
// Shell: owns width/background/padding and holds the pinned header
// (title + histogram). It does NOT scroll.
static View buildSidebar(const std::vector<Activity> &activities, View &maskEditor) {
  View sidebarShell;
  sidebarShell.style.height = Size::full();
  sidebarShell.style.flexShrink = 0;
  sidebarShell.style.direction = FlexDirection::Column;
  sidebarShell.style.backgroundColor = kColorPanelBg;
  sidebarShell.style.padding = EdgeInsets::all(12.0f);
  sidebarShell.style.overflowX = Overflow::Hidden;
  sidebarShell.style.overflowY = Overflow::Visible;
  sidebarShell.style.width = std::function<Size()>([] {
    return Size::pixel(state.activeActivity != kNoActivity ? state.sidebarWidth : 0.0f);
  });

  // Body: all the panes live here, and this is the part that scrolls.
  View sidebar;
  sidebar.style.width = Size::full();
  sidebar.style.flexGrow = 1;
  sidebar.style.minHeight = 0;
  sidebar.style.direction = FlexDirection::Column;
  sidebar.style.overflowX = Overflow::Hidden;
  sidebar.style.overflowY = Overflow::Auto;

  Text sidebarTitle;
  sidebarTitle.label =
      std::function<std::string()>([activities]() -> std::string {
        for (const auto &a : activities)
          if (a.id == state.activeActivity)
            return a.title;
        return "";
      });
  sidebarTitle.fontSize = 12.0f;
  sidebarTitle.fontWeight = FontWeight::SemiBold;
  sidebarTitle.color = kColorTextMuted;
  sidebarTitle.style.margin = EdgeInsets{0, 0, 10, 0};
  sidebarTitle.style.flexShrink = 0;
  sidebarShell.addChild(std::move(sidebarTitle));

  sidebarShell.addChild(buildHistogramView());

  sidebar.addChild(buildInfoPane());
  sidebar.addChild(buildHistoryPane());
  sidebar.addChild(buildSnapshotsPane());
  sidebar.addChild(buildAdjustPane());
  sidebar.addChild(buildPresetsPane());
  sidebar.addChild(buildLutsPane());
  sidebar.addChild(buildCurvePane());
  sidebar.addChild(buildMixPane());
  sidebar.addChild(buildGradePane());
  MaskPaneViews masks = buildMaskPanes();
  sidebar.addChild(std::move(masks.pane));
  maskEditor = std::move(masks.editor);
  sidebar.addChild(buildCropPane());
  sidebar.addChild(buildRotatePane());
  sidebar.addChild(buildExportPane());

  sidebarShell.addChild(std::move(sidebar));

  return sidebarShell;
}
