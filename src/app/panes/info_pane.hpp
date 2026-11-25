// app/panes/info_pane.hpp: the Info pane (buildInfoPane)
#pragma once

#include <functional> // std::function
#include <string>     // std::string, std::to_string
#include <utility>    // std::move

#include "liteui.hpp" // View, Text, Size, FlexDirection, Display

#include "app/constants.hpp"    // kInfoActivity, kColorTextMid, kColorTextMuted
#include "app/editor_state.hpp" // inline EditorState state
#include "app/saved_edits.hpp" // pictureLabel, currentColorTag, currentPhotoInfo
#include "io/exif.hpp"         // exif::describeCamera, describeExposure,
                               // describeFocalLength

// -- "Info" pane: real data about the open image.
inline View buildInfoPane() {
  View infoPane;
  infoPane.style.direction = FlexDirection::Column;
  infoPane.style.width = Size::full();
  infoPane.style.gap = 6.0f;
  infoPane.style.display = std::function<Display()>([] {
    return state.activeActivity == kInfoActivity ? Display::Flex
                                                 : Display::None;
  });

  Text infoName;
  infoName.label = std::function<std::string()>([] {
    return state.image ? ("File: " + pictureLabel(state.imagePath))
                       : std::string("No image open");
  });
  infoName.fontSize = 13.0f;
  infoName.color = kColorTextMid;
  infoPane.addChild(std::move(infoName));

  Text infoDims;
  infoDims.label = std::function<std::string()>([] {
    if (!state.image)
      return std::string();
    return "Size: " + std::to_string(state.image->width) + " x " +
           std::to_string(state.image->height) + " px";
  });
  infoDims.fontSize = 13.0f;
  infoDims.color = kColorTextMid;
  infoPane.addChild(std::move(infoDims));

  Text infoZoom;
  infoZoom.label = std::function<std::string()>([] {
    return "Zoom: " +
           std::to_string(static_cast<int>(state.zoomDev * 100.0f + 0.5f)) +
           "%";
  });
  infoZoom.fontSize = 13.0f;
  infoZoom.color = kColorTextMid;
  infoPane.addChild(std::move(infoZoom));

  Text infoColor;
  infoColor.label = std::function<std::string()>([] {
    if (!state.image)
      return std::string();
    const std::string &t = currentColorTag().text;
    return t.empty() ? t : "Color: " + t;
  });
  infoColor.fontSize = 13.0f;
  infoColor.color = kColorTextMid;
  infoColor.style.display = std::function<Display()>([] {
    return state.image && !currentColorTag().text.empty() ? Display::Flex
                                                          : Display::None;
  });
  infoPane.addChild(std::move(infoColor));

  // Camera data from the file's EXIF block. Each row hides itself when the file
  // doesn't say; a picture with none of it gets one muted line instead. GPS is
  // never shown.
  const auto infoRow = [](std::function<std::string()> text,
                          bool muted = false) {
    Text row;
    row.label = text;
    row.fontSize = 13.0f;
    row.color = muted ? kColorTextMuted : kColorTextMid;
    row.style.display = std::function<Display()>(
        [text] { return text().empty() ? Display::None : Display::Flex; });
    return row;
  };
  infoPane.addChild(infoRow([]() -> std::string {
    if (!state.image)
      return {};
    const std::string s = exif::describeCamera(currentPhotoInfo());
    return s.empty() ? s : "Camera: " + s;
  }));
  infoPane.addChild(infoRow([]() -> std::string {
    if (!state.image)
      return {};
    const std::string &s = currentPhotoInfo().lens;
    return s.empty() ? s : "Lens: " + s;
  }));
  infoPane.addChild(infoRow([]() -> std::string {
    if (!state.image)
      return {};
    const std::string s = exif::describeExposure(currentPhotoInfo());
    return s.empty() ? s : "Exposure: " + s;
  }));
  infoPane.addChild(infoRow([]() -> std::string {
    if (!state.image)
      return {};
    const std::string s =
        exif::describeFocalLength(currentPhotoInfo().focalLength);
    return s.empty() ? s : "Focal length: " + s;
  }));
  infoPane.addChild(infoRow([]() -> std::string {
    if (!state.image)
      return {};
    const std::string &s = currentPhotoInfo().dateTaken;
    return s.empty() ? s : "Taken: " + s;
  }));
  infoPane.addChild(infoRow(
      []() -> std::string {
        return state.image && currentPhotoInfo().empty()
                   ? "No camera data in this file"
                   : std::string();
      },
      true));

  return infoPane;
}
