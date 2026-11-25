// app/panes/presets_pane.hpp: the Presets pane (buildPresetsPane)
#pragma once

#include <cstddef>    // size_t
#include <functional> // std::function
#include <string>     // std::string
#include <utility>    // std::move

#include "liteui.hpp" // View, Text, TextInput, Size, FlexDirection, Display,
                      // EdgeInsets, Color

#include "app/constants.hpp"    // kPresetsActivity, kPresetRows, kColor*
#include "app/editor_state.hpp" // inline EditorState state
#include "app/mask_widgets.hpp" // buildPresetRow
#include "app/presets.hpp"      // refreshPresets, filterPresetNameField,
                                // saveCurrentPreset, deleteSelectedPreset
#include "app/ui_state.hpp"     // uiState (.names, .presets)
#include "app/widgets.hpp"      // sidebarButton, registerTextField
#include "io/preset.hpp"        // presetDir
// -- "Presets" pane: saved looks. Click one to apply it (one undo step);
// type a name and Save to store the current look. The list is re-read from
// disk each time the pane is opened.
inline View buildPresetsPane() {
  View presetsPane;
  presetsPane.style.width = Size::full();
  presetsPane.style.direction = FlexDirection::Column;
  presetsPane.style.gap = 6.0f;
  presetsPane.style.display = std::function<Display()>([] {
    static bool wasShown = false;
    const bool shown = state.activeActivity == kPresetsActivity;
    if (shown && !wasShown)
      refreshPresets();
    wasShown = shown;
    return shown ? Display::Flex : Display::None;
  });

  Text presetsHint;
  presetsHint.label = std::string(
      "A preset is a look: sliders, curves, Color Mix, Color Grading and the "
      "LUT. "
      "Crop, rotation and masks are not included. Applying one replaces the "
      "current look and can be undone.");
  presetsHint.fontSize = 12.0f;
  presetsHint.color = kColorTextMuted;
  presetsHint.style.margin = EdgeInsets{0, 0, 6, 0};
  presetsPane.addChild(std::move(presetsHint));

  {
    TextInput ti;
    ti.placeholder = "Name this look";
    ti.fontSize = 13.0f;
    ti.textColor = kColorTextDark;
    ti.placeholderColor = kColorTextMuted;
    ti.caretColor = kColorTextDark;
    ti.borderColor = kColorDividerHover;
    ti.focusedBorderColor = Color{90, 150, 235, 255};
    ti.style.width = Size::full();
    ti.style.height = Size::pixel(28);
    ti.style.flexShrink = 0;
    ti.style.borderWidth = 1.0f;
    ti.style.borderRadius = 4.0f;
    ti.style.backgroundColor = kColorActivityBarBg;
    ti.onChange = [](const std::string &s) { filterPresetNameField(s); };
    ti.onSubmit = [](const std::string &) {
      saveCurrentPreset();
    }; // Enter saves, like the button
    uiState.names.preset = ti.state;
    presetsPane.addChild(std::move(ti));
    registerTextField(uiState.names.preset, presetsPane.children.back());
  }
  presetsPane.addChild(sidebarButton("Save current look", saveCurrentPreset, {},
                                     true,
                                     [] { return state.original != nullptr; }));

  Text presetsEmpty;
  presetsEmpty.label =
      std::string("No presets yet. Name the current look and save it.");
  presetsEmpty.fontSize = 12.0f;
  presetsEmpty.color = kColorTextMuted;
  presetsEmpty.style.margin = EdgeInsets{6, 0, 0, 0};
  presetsEmpty.style.display = std::function<Display()>([] {
    return uiState.presets.list.empty() ? Display::Flex : Display::None;
  });
  presetsPane.addChild(std::move(presetsEmpty));

  for (size_t i = 0; i < kPresetRows; ++i)
    presetsPane.addChild(buildPresetRow(i));

  presetsPane.addChild(sidebarButton(
      "Delete selected preset", deleteSelectedPreset, {}, false, [] {
        return uiState.presets.sel >= 0 &&
               static_cast<size_t>(uiState.presets.sel) <
                   uiState.presets.list.size();
      }));

  Text presetsFolder;
  presetsFolder.label = "Saved in " + presetDir();
  presetsFolder.fontSize = 11.0f;
  presetsFolder.color = kColorTextMuted;
  presetsFolder.style.margin = EdgeInsets{8, 0, 0, 0};
  presetsPane.addChild(std::move(presetsFolder));

  return presetsPane;
}
