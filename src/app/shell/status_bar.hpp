// app/shell/status_bar.hpp: the status bar (buildStatusBar)
#pragma once

#include <functional> // std::function
#include <string>     // std::string, std::to_string
#include <utility>    // std::move

#include "liteui.hpp" // View, Text, Size, EdgeInsets, Align, Justify,
                      // FlexDirection, TextWrap, Color

#include "app/canvas_view.hpp" // setZoom
#include "app/constants.hpp"   // kStatusBarH, kZoomStep, kColorStatusBarBg,
    // kColorStatusHover, kColorStatusError, kColorWhite
#include "app/editor_state.hpp" // inline EditorState state, CompareMode
#include "app/saved_edits.hpp"  // pictureLabel

// Status bar: image info on the left, zoom controls on the right.
inline View buildStatusBar() {
  View statusBar;
  statusBar.style.width = Size::full();
  statusBar.style.height = Size::pixel(kStatusBarH);
  statusBar.style.flexShrink = 0;
  statusBar.style.direction = FlexDirection::Row;
  statusBar.style.alignItems = Align::Center;
  statusBar.style.justifyContent = Justify::SpaceBetween;
  statusBar.style.backgroundColor = kColorStatusBarBg;
  statusBar.style.padding = EdgeInsets{0, 10, 0, 12};

  Text statusLeft;
  statusLeft.label = std::function<std::string()>([] {
    std::string s = state.image
                        ? pictureLabel(state.imagePath) + "  —  " +
                              std::to_string(state.image->width) + " x " +
                              std::to_string(state.image->height) + " px"
                        : std::string("No image open");
    if (state.image && state.compare == CompareMode::Before)
      s += "     •     Before (press \\ to return)";
    else if (state.image && state.compare == CompareMode::Split)
      s += "     •     Before | After (Y to close, drag to move the line)";
    if (!state.statusMsg.empty())
      s += "     •     " + state.statusMsg;
    return s;
  });
  statusLeft.fontSize = 12.0f;
  statusLeft.wrap = TextWrap::NoWrap; // one line, even with a long message
  statusLeft.color = std::function<Color()>(
      [] { return state.statusIsError ? kColorStatusError : kColorWhite; });
  statusBar.addChild(std::move(statusLeft));

  // Right side of the status bar: zoom controls.
  auto statusGlyphBtn = [](const std::string &label,
                           std::function<void()> onClick) {
    View v;
    v.style.width = Size::pixel(20);
    v.style.height = Size::pixel(20);
    v.style.borderRadius = 3.0f;
    v.style.justifyContent = Justify::Center;
    v.style.alignItems = Align::Center;
    v.style.hoverColor = kColorStatusHover;
    v.onClick = std::move(onClick);
    Text t;
    t.label = label;
    t.fontSize = 12.0f;
    t.color = kColorWhite;
    v.addChild(std::move(t));
    return v;
  };

  View zoomControls;
  zoomControls.style.direction = FlexDirection::Row;
  zoomControls.style.alignItems = Align::Center;
  zoomControls.style.gap = 4.0f;

  // RGB under the cursor: swatch + values. Fixed width so the zoom controls
  // to its right never shift as the numbers change.
  View rgbReadout;
  rgbReadout.style.width = Size::pixel(176);
  rgbReadout.style.direction = FlexDirection::Row;
  rgbReadout.style.alignItems = Align::Center;
  rgbReadout.style.gap = 6.0f;
  rgbReadout.style.margin = EdgeInsets{0, 14, 0, 0};

  View rgbSwatch;
  rgbSwatch.style.width = Size::pixel(12);
  rgbSwatch.style.height = Size::pixel(12);
  rgbSwatch.style.flexShrink = 0;
  rgbSwatch.style.borderRadius = 2.0f;
  rgbSwatch.style.backgroundColor = std::function<Color()>([]() -> Color {
    return state.hoverValid
               ? Color{state.hoverR, state.hoverG, state.hoverB, 255}
               : Color{0, 0, 0, 0};
  });
  rgbReadout.addChild(std::move(rgbSwatch));

  Text rgbText;
  rgbText.label = std::function<std::string()>([] {
    if (!state.hoverValid)
      return std::string();
    return "R " + std::to_string(state.hoverR) + "  G " +
           std::to_string(state.hoverG) + "  B " + std::to_string(state.hoverB);
  });
  rgbText.fontSize = 12.0f;
  rgbText.wrap = TextWrap::NoWrap;
  rgbText.color = kColorWhite;
  rgbReadout.addChild(std::move(rgbText));
  zoomControls.addChild(std::move(rgbReadout));

  zoomControls.addChild(
      statusGlyphBtn("-", [] { setZoom(state.zoomDev / kZoomStep); }));

  Text zoomLabel;
  zoomLabel.label = [] {
    return std::to_string(static_cast<int>(state.zoomDev * 100.0f + 0.5f)) +
           "%";
  };
  zoomLabel.fontSize = 12.0f;
  zoomLabel.color = kColorWhite;
  zoomLabel.style.margin = EdgeInsets{0, 4, 0, 4};
  zoomControls.addChild(std::move(zoomLabel));

  zoomControls.addChild(
      statusGlyphBtn("+", [] { setZoom(state.zoomDev * kZoomStep); }));

  View zoomResetSpacer;
  zoomResetSpacer.style.width = Size::pixel(6);
  zoomControls.addChild(std::move(zoomResetSpacer));

  View zoomResetBtn;
  zoomResetBtn.style.justifyContent = Justify::Center;
  zoomResetBtn.style.alignItems = Align::Center;
  zoomResetBtn.style.hoverColor = kColorStatusHover;
  zoomResetBtn.style.borderRadius = 3.0f;
  zoomResetBtn.style.padding = EdgeInsets{2, 6, 2, 6};
  zoomResetBtn.onClick = [] { setZoom(1.0f); };
  {
    Text t;
    t.label = std::string("100%");
    t.fontSize = 12.0f;
    t.color = kColorWhite;
    zoomResetBtn.addChild(std::move(t));
  }
  zoomControls.addChild(std::move(zoomResetBtn));

  statusBar.addChild(std::move(zoomControls));

  return statusBar;
}
