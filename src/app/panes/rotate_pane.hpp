// app/panes/rotate_pane.hpp: the Rotate pane (buildRotatePane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "Rotate" pane: 90-degree turns, mirrors, and a reset. Lossless (pure
// pixel moves) and each button press is one undo step.
static View buildRotatePane() {
  View rotatePane;
  rotatePane.style.width = Size::full();
  rotatePane.style.direction = FlexDirection::Column;
  rotatePane.style.gap = 6.0f;
  rotatePane.style.display = std::function<Display()>(
      [] { return state.activeActivity == kRotateActivity ? Display::Flex : Display::None; });

  Text rotateHint;
  rotateHint.label = std::function<std::string()>([]() -> std::string {
    return state.image ? "Turn or flip the whole image, or straighten it with "
                         "the angle slider. Straightening clears any crop."
                       : "Open an image to rotate it.";
  });
  rotateHint.fontSize = 12.0f;
  rotateHint.color = kColorTextMuted;
  rotateHint.style.margin = EdgeInsets{0, 0, 6, 0};
  rotatePane.addChild(std::move(rotateHint));

  const auto geoIdle = [] { return !geometryBusy(); };
  rotatePane.addChild(sidebarButton(
      "\u21BA  Rotate Left 90\u00B0", [] { applyGeoOp(GeoOp::RotateCCW); }, {},
      false, geoIdle));
  rotatePane.addChild(sidebarButton(
      "\u21BB  Rotate Right 90\u00B0", [] { applyGeoOp(GeoOp::RotateCW); }, {},
      false, geoIdle));
  rotatePane.addChild(sidebarButton(
      "\u2194  Flip Horizontal", [] { applyGeoOp(GeoOp::FlipH); }, {}, false,
      geoIdle));
  rotatePane.addChild(sidebarButton(
      "\u2195  Flip Vertical", [] { applyGeoOp(GeoOp::FlipV); }, {}, false,
      geoIdle));
  rotatePane.addChild(sidebarButton("Reset Turns & Flips", resetOrientation, {},
                                    false, geoIdle));

  // ---- Straighten: arbitrary angle ----
  Text straightenTitle;
  straightenTitle.label = std::string("Straighten");
  straightenTitle.fontSize = 12.0f;
  straightenTitle.fontWeight = FontWeight::SemiBold;
  straightenTitle.color = kColorTextMuted;
  straightenTitle.style.margin = EdgeInsets{10, 0, 0, 0};
  rotatePane.addChild(std::move(straightenTitle));

  SliderSpec angleSlider;
  angleSlider.name = "Angle";
  angleSlider.lo = -kAngleMax;
  angleSlider.hi = kAngleMax;
  angleSlider.get = [] { return state.adjust.angle(); };
  angleSlider.set = setAngleFromSlider;
  angleSlider.format = [](float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f\u00B0", static_cast<double>(v));
    return std::string(buf);
  };
  angleSlider.isAngle = true;
  rotatePane.addChild(buildSliderRow(angleSlider));

  // Fine adjustment: the track is ~0.6 degrees per pixel, too coarse for
  // levelling a horizon, so step buttons sit right under it.
  auto nudgeBtn = [](const std::string &label, float delta) {
    View v;
    v.style.width = Size::pixel(36);
    v.style.height = Size::pixel(26);
    v.style.flexShrink = 0;
    v.style.borderRadius = 4.0f;
    v.style.justifyContent = Justify::Center;
    v.style.alignItems = Align::Center;
    v.style.backgroundColor = kColorActivityBarBg;
    v.style.hoverColor = kColorActivityHover;
    v.onClick = [delta] { nudgeAngle(delta); };
    Text t;
    t.label = label;
    t.fontSize = 11.0f;
    t.color = kColorTextDark;
    v.addChild(std::move(t));
    return v;
  };
  View nudgeRow;
  nudgeRow.style.direction = FlexDirection::Row;
  nudgeRow.style.gap = 3.0f;
  nudgeRow.addChild(nudgeBtn("-1\u00B0", -1.0f));
  nudgeRow.addChild(nudgeBtn("-0.1\u00B0", -0.1f));
  nudgeRow.addChild(nudgeBtn("+0.1\u00B0", 0.1f));
  nudgeRow.addChild(nudgeBtn("+1\u00B0", 1.0f));
  rotatePane.addChild(std::move(nudgeRow));

  Text cornersTitle;
  cornersTitle.label = std::string("Corners");
  cornersTitle.fontSize = 12.0f;
  cornersTitle.fontWeight = FontWeight::SemiBold;
  cornersTitle.color = kColorTextMuted;
  cornersTitle.style.margin = EdgeInsets{4, 0, 0, 0};
  rotatePane.addChild(std::move(cornersTitle));

  rotatePane.addChild(sidebarButton(
      "Auto-crop empty corners", [] { setCornerMode(false); },
      [] { return !state.adjust.expand(); }));
  rotatePane.addChild(sidebarButton(
      "Keep full canvas", [] { setCornerMode(true); },
      [] { return state.adjust.expand(); }));
  rotatePane.addChild(
      sidebarButton("Reset Angle", [] { setAngleDiscrete(0.0f); }));

  return rotatePane;
}
