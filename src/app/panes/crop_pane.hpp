// app/panes/crop_pane.hpp: the Crop pane (buildCropPane)
#pragma once

#include <cmath>      // std::lround
#include <cstddef>    // size_t
#include <functional> // std::function
#include <string>     // std::string, std::to_string
#include <utility>    // std::move

#include "liteui.hpp" // View, Text, Size, FlexDirection, Display,
                      // EdgeInsets, FontWeight

#include "app/constants.hpp"    // kCropActivity, kColorTextMuted, kColorTextMid
#include "app/crop.hpp"         // resetCropSelection, setCropAspect
#include "app/crop_tool.hpp"    // applyCrop, removeCrop
#include "app/editor_state.hpp" // inline EditorState state, kCropAspects
#include "app/geometry_worker.hpp" // geometryBusy
#include "app/widgets.hpp"         // sidebarButton

// -- "Crop" pane: aspect-ratio presets, a live readout, and the
// Apply/Reset/Remove buttons. The selection itself is drawn and dragged
// directly on the canvas (paintCropOverlay / cropPress / cropDrag).
inline View buildCropPane() {
  View cropPane;
  cropPane.style.width = Size::full();
  cropPane.style.direction = FlexDirection::Column;
  cropPane.style.gap = 6.0f;
  cropPane.style.display = std::function<Display()>([] {
    return state.activeActivity == kCropActivity ? Display::Flex
                                                 : Display::None;
  });

  Text cropHint;
  cropHint.label = std::function<std::string()>([]() -> std::string {
    return state.image
               ? "Drag on the image to choose an area. Drag the handles to "
                 "resize, or the inside to move."
               : "Open an image to crop it.";
  });
  cropHint.fontSize = 12.0f;
  cropHint.color = kColorTextMuted;
  cropHint.style.margin = EdgeInsets{0, 0, 6, 0};
  cropPane.addChild(std::move(cropHint));

  Text cropAspectTitle;
  cropAspectTitle.label = std::string("Aspect ratio");
  cropAspectTitle.fontSize = 12.0f;
  cropAspectTitle.fontWeight = FontWeight::SemiBold;
  cropAspectTitle.color = kColorTextMuted;
  cropPane.addChild(std::move(cropAspectTitle));

  for (size_t i = 0; i < kCropAspects.size(); ++i)
    cropPane.addChild(sidebarButton(
        kCropAspects[i].label, [i] { setCropAspect(i); },
        [i] { return state.crop.aspectIdx == i; }));

  Text cropInfo;
  cropInfo.label = std::function<std::string()>([]() -> std::string {
    if (!state.image)
      return std::string();
    const int w = static_cast<int>(std::lround(state.crop.r - state.crop.l));
    const int h = static_cast<int>(std::lround(state.crop.b - state.crop.t));
    return "Selection: " + std::to_string(w) + " x " + std::to_string(h) +
           " px";
  });
  cropInfo.fontSize = 13.0f;
  cropInfo.color = kColorTextMid;
  cropInfo.style.margin = EdgeInsets{8, 0, 4, 0};
  cropPane.addChild(std::move(cropInfo));

  cropPane.addChild(sidebarButton("Apply Crop", applyCrop, {}, true,
                                  [] { return !geometryBusy(); }));
  cropPane.addChild(sidebarButton("Reset Selection", [] {
    resetCropSelection();
    state.markDirty();
  }));
  cropPane.addChild(sidebarButton("Remove Crop", removeCrop, {}, false,
                                  [] { return !geometryBusy(); }));

  return cropPane;
}
