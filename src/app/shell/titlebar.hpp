// app/shell/titlebar.hpp: the title bar (buildTitlebar)
#pragma once

#include <string>  // std::string, std::to_string
#include <utility> // std::move
#include <vector>  // std::vector

#include "liteui.hpp" // LiteUI, View, Text, Size, Align, FlexDirection, EdgeInsets,
                      // FontWeight, PointerEvent, MouseButton

#include "app/auto_tone_hist.hpp" // autoTone, toggleClipping, toggleChannelClipping
#include "app/canvas_view.hpp" // setZoom
#include "app/compare.hpp"     // toggleBefore, toggleSplit
#include "app/constants.hpp" // kTitlebarH, kZoomStep, kColorTitlebarBg, kColorTextDark,
                             // kColorMenuHover, kColorCloseHover
#include "app/editor_state.hpp" // inline EditorState state
#include "app/export.hpp"       // exportSelected, exportAllShown, stopExport
#include "app/filmstrip.hpp" // filmStep, filmSelectAll, toggleFilmstrip, openFolder,
                             // starsText, kLabelDisplay
#include "app/meta_commands.hpp" // openImage, exportImage, toggleSidebar, ratePhoto,
                                 // flagPhoto, labelPhoto
#include "app/presets.hpp"       // copySettings, pasteSettings
#include "app/rotate.hpp"        // undo, redo, applyGeoOp
#include "app/sync.hpp"          // syncSettings
#include "app/virtual_copies.hpp" // createVirtualCopy, deleteVirtualCopy
#include "app/widgets.hpp"   // MenuItem, menuButton, winButton, titlebarIconBtn
#include "core/geometry.hpp" // GeoOp
#include "io/photo_meta.hpp" // PhotoFlag, kMaxRating, kLabelCount
// Titlebar: title, File/View menus, window controls.
inline View buildTitlebar(LiteUI &ui) {
  View titlebar;
  titlebar.style.direction = FlexDirection::Row;
  titlebar.style.width = Size::full();
  titlebar.style.height = Size::pixel(kTitlebarH);
  titlebar.style.flexShrink = 0;
  titlebar.style.alignItems = Align::Center;
  titlebar.style.backgroundColor = kColorTitlebarBg;
  titlebar.style.borderWidth = 0.0f;
  // Empty space in the titlebar (i.e. not over a button) drags the
  // window, and also closes any open dropdown menu.
  titlebar.onPointerDown = [&](PointerEvent &e) {
    if (e.button != MouseButton::Left)
      return;
    e.stopPropagation();
    state.openMenu = -1;
    ui.requestMove();
  };

  Text appTitle;
  appTitle.label = std::string("Lite Room");
  appTitle.fontSize = 13.0f;
  appTitle.fontWeight = FontWeight::SemiBold;
  appTitle.color = kColorTextDark;
  appTitle.style.margin = EdgeInsets{0, 16, 0, 14};
  titlebar.addChild(std::move(appTitle));

  titlebar.addChild(
      menuButton("File", 0,
                 {
                     {"Open Image...", openImage, "Ctrl+O"},
                     {"Open Folder...", openFolder, "Ctrl+Shift+O"},
                     {"Export...", exportImage, "Ctrl+E"},
                     {"Export Selected...", exportSelected, "Ctrl+Shift+E"},
                     {"Export All Shown...", exportAllShown},
                     {"Stop Export", stopExport},
                     {"Exit", [&] { ui.requestClose(); }, "Ctrl+Q"},
                 }));
  titlebar.addChild(menuButton(
      "View", 1,
      {
          {"Zoom In", [] { setZoom(state.zoomDev * kZoomStep); }, "Ctrl+="},
          {"Zoom Out", [] { setZoom(state.zoomDev / kZoomStep); }, "Ctrl+-"},
          {"Actual Size (100%)", [] { setZoom(1.0f); }, "Ctrl+0"},
          {"Toggle Sidebar", toggleSidebar, "Ctrl+B"},
          {"Toggle Filmstrip", toggleFilmstrip, "F6"},
          {"Previous Image", [] { filmStep(-1); }, "Left"},
          {"Next Image", [] { filmStep(1); }, "Right"},
          {"Before / After", toggleBefore, "\\"},
          {"Before | After Split", toggleSplit, "Y"},
          {"Clipping Warnings", toggleClipping, "J"},
          {"Clipping: Per-Channel Colors", toggleChannelClipping, "Shift+J"},
      }));

  titlebar.addChild(menuButton(
      "Image", 2,
      {
          {"Rotate Left 90\u00B0", [] { applyGeoOp(GeoOp::RotateCCW); },
           "Ctrl+["},
          {"Rotate Right 90\u00B0", [] { applyGeoOp(GeoOp::RotateCW); },
           "Ctrl+]"},
          {"Flip Horizontal", [] { applyGeoOp(GeoOp::FlipH); }},
          {"Flip Vertical", [] { applyGeoOp(GeoOp::FlipV); }},
          {"Auto Tone", autoTone, "A"},
          {"Copy Settings", copySettings, "Ctrl+Shift+C"},
          {"Paste Settings", pasteSettings, "Ctrl+Shift+V"},
          {"Select All Photos", filmSelectAll, "Ctrl+A"},
          {"Sync Settings to Selected", syncSettings, "Ctrl+Shift+S"},
          {"Create Virtual Copy", createVirtualCopy, "Ctrl+Shift+N"},
          {"Delete Virtual Copy", deleteVirtualCopy},
      }));

  { // Photo: rating, flag and color label of the open picture (see ratePhoto)
    std::vector<MenuItem> photo;
    for (int s = 1; s <= kMaxRating; ++s)
      photo.push_back(
          {"Rate " + starsText(s), [s] { ratePhoto(s); }, std::to_string(s)});
    photo.push_back({"Clear Rating", [] { ratePhoto(0); }, "0"});
    photo.push_back({"Flag as Pick", [] { flagPhoto(PhotoFlag::Pick); }, "P"});
    photo.push_back(
        {"Flag as Rejected", [] { flagPhoto(PhotoFlag::Reject); }, "X"});
    photo.push_back({"Remove Flag", [] { flagPhoto(PhotoFlag::None); }, "U"});
    for (int c = 1; c <= kLabelCount; ++c)
      photo.push_back({std::string("Label ") + kLabelDisplay[c - 1],
                       [c] { labelPhoto(c); },
                       c <= 4 ? std::to_string(5 + c) : std::string()});
    photo.push_back({"Clear Label", [] { labelPhoto(0); }});
    titlebar.addChild(menuButton("Photo", 3, std::move(photo)));
  }

  View undoRedoGap;
  undoRedoGap.style.width = Size::pixel(8);
  titlebar.addChild(std::move(undoRedoGap));

  titlebar.addChild(
      titlebarIconBtn("\u21B6", undo, [] { return state.adjust.canUndo(); }));
  titlebar.addChild(
      titlebarIconBtn("\u21B7", redo, [] { return state.adjust.canRedo(); }));

  View titlebarSpacer;
  titlebarSpacer.style.flexGrow = 1;
  titlebarSpacer.style.height = Size::full();
  titlebar.addChild(std::move(titlebarSpacer));

  titlebar.addChild(
      winButton("\u2500", kColorMenuHover, [&] { ui.requestMinimize(); }));
  titlebar.addChild(winButton("\u25A1", kColorMenuHover, [&] {
    ui.requestMaximize();
    state.maximized = !state.maximized;
  }));
  titlebar.addChild(
      winButton("\u00D7", kColorCloseHover, [&] { ui.requestClose(); }));

  return titlebar;
}
