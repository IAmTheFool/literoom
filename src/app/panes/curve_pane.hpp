// app/panes/curve_pane.hpp: the Curve pane (buildCurvePane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "Curve" pane: tone curves — RGB (master) plus per-channel R, G, B, one
// tab each. Click the plot to add a point, drag to move it, right-click a
// point to delete it. The whole gesture (add + drag, or a plain drag) is one
// undo step; the edited channel's histogram sits behind the curve, and
// other channels' curves that have been changed stay visible, dimmed.
static View buildCurvePane() {
  View curvePane;
  curvePane.style.width = Size::full();
  curvePane.style.direction = FlexDirection::Column;
  curvePane.style.gap = 8.0f;
  curvePane.style.display = std::function<Display()>([] {
    return state.activeActivity == kCurveActivity ? Display::Flex
                                                  : Display::None;
  });

  // Channel tabs. A bullet after a name marks a curve that has been changed,
  // so an edit on a channel you've since switched away from isn't forgotten.
  View curveTabs;
  curveTabs.style.direction = FlexDirection::Row;
  curveTabs.style.gap = 4.0f;
  curveTabs.style.width = Size::pixel(kCurveSizePx);
  curveTabs.style.flexShrink = 0;
  for (int c = 0; c < static_cast<int>(kCurveChannelCount); ++c) {
    const auto ch = static_cast<CurveChannel>(c);
    View tab;
    tab.style.width = Size::pixel(kCurveTabW);
    tab.style.height = Size::pixel(26);
    tab.style.flexShrink = 0;
    tab.style.borderRadius = 4.0f;
    tab.style.justifyContent = Justify::Center;
    tab.style.alignItems = Align::Center;
    tab.style.hoverColor = kColorActivityHover;
    tab.style.backgroundColor = std::function<Color()>([ch]() -> Color {
      return uiState.curve.channel == ch ? kColorDropdownRowHover
                                         : kColorActivityBarBg;
    });
    tab.style.borderWidth = std::function<float()>(
        [ch] { return uiState.curve.channel == ch ? 1.0f : 0.0f; });
    tab.style.borderColor = kColorDividerHover;
    tab.onClick = [ch] { setCurveChannel(ch); };

    Text tabText;
    tabText.label = std::function<std::string()>([ch]() -> std::string {
      std::string name = ch == CurveChannel::Master  ? "RGB"
                         : ch == CurveChannel::Red   ? "R"
                         : ch == CurveChannel::Green ? "G"
                                                     : "B";
      if (!(state.adjust.curve(ch) == identityCurve()))
        name += " \u2022";
      return name;
    });
    tabText.fontSize = 12.0f;
    tabText.fontWeight = FontWeight::SemiBold;
    tabText.color = ch == CurveChannel::Master  ? kColorTextDark
                    : ch == CurveChannel::Red   ? Color{235, 95, 95, 255}
                    : ch == CurveChannel::Green ? Color{95, 200, 115, 255}
                                                : Color{115, 155, 255, 255};
    tab.addChild(std::move(tabText));
    curveTabs.addChild(std::move(tab));
  }
  curvePane.addChild(std::move(curveTabs));

  View curveCanvas;
  curveCanvas.isCanvas = true;
  curveCanvas.style.width = Size::pixel(kCurveSizePx);
  curveCanvas.style.height = Size::pixel(kCurveSizePx);
  curveCanvas.style.flexShrink = 0;
  curveCanvas.onPaint = [](CanvasContext &ctx) { paintCurve(ctx); };
  curveCanvas.canvasDirtySource = [] {
    static uint32_t lastPainted = 0;
    static int lastActivity = kNeverSeenActivity;
    // Every image recompute bumps histVersion, which covers curve edits,
    // undo/redo, and a newly opened image.
    refreshHistogramIfStale();
    const bool tabChanged = state.activeActivity != lastActivity;
    lastActivity = state.activeActivity;
    const bool uiDirty =
        uiState.curve.uiDirty; // channel switch / point selection
    uiState.curve.uiDirty = false;
    if (state.histVersion == lastPainted && !tabChanged && !uiDirty)
      return false;
    lastPainted = state.histVersion;
    return true;
  };
  bindDrag(
      curveCanvas, [](float lx, float ly) { curvePress(lx, ly); },
      [](float lx, float ly) { curveDrag(lx, ly); }, [] { curveEnd(); },
      [](float lx, float ly) { curveRightPress(lx, ly); }, Cursor::Crosshair);
  curvePane.addChild(std::move(curveCanvas));

  Text curveInfo;
  curveInfo.label = std::function<std::string()>([]() -> std::string {
    const auto &p = curveActiveCurve();
    const std::string name = curveChannelName(uiState.curve.channel);
    if (uiState.curve.active >= 0 &&
        uiState.curve.active < static_cast<int>(p.size()))
      return name + ": In " +
             std::to_string(
                 static_cast<int>(std::lround(p[uiState.curve.active].x))) +
             "  Out " +
             std::to_string(
                 static_cast<int>(std::lround(p[uiState.curve.active].y)));
    return name + ": " + std::to_string(p.size()) + " points";
  });
  curveInfo.fontSize = 13.0f;
  curveInfo.color = kColorTextMid;
  curvePane.addChild(std::move(curveInfo));

  Text curveHint1;
  curveHint1.label = std::string("Click: add point   Drag: move");
  curveHint1.fontSize = 11.0f;
  curveHint1.color = kColorTextFaint;
  curvePane.addChild(std::move(curveHint1));
  Text curveHint2;
  curveHint2.label = std::string("Right-click: delete point");
  curveHint2.fontSize = 11.0f;
  curveHint2.color = kColorTextFaint;
  curvePane.addChild(std::move(curveHint2));

  curvePane.addChild(sidebarButton("Reset Curve", resetCurve));
  curvePane.addChild(sidebarButton("Reset All Curves", resetAllCurves));

  return curvePane;
}
