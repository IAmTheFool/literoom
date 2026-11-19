// app/panes/snapshots_pane.hpp: the Snapshots pane (buildSnapshotsPane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "Snapshots" pane: named copies of every edit on this picture. Click one
// to restore it (one undo step); type a name and Save to keep the current
// state. They are saved in the picture's edits file, not in the presets
// folder.
static View buildSnapshotsPane() {
  View snapshotsPane;
  snapshotsPane.style.width = Size::full();
  snapshotsPane.style.direction = FlexDirection::Column;
  snapshotsPane.style.gap = 6.0f;
  snapshotsPane.style.display = std::function<Display()>([] {
    return state.activeActivity == kSnapshotsActivity ? Display::Flex
                                                      : Display::None;
  });

  Text snapshotsHint;
  snapshotsHint.label = std::function<std::string()>([]() -> std::string {
    return state.original
               ? "A snapshot is a named copy of every edit on this picture: "
                 "sliders, curves, Color Mix, Color Grading, crop, rotation "
                 "and masks. Restoring one replaces all of them and can be "
                 "undone. Snapshots are saved with the picture's edits."
               : "Open an image to keep snapshots of its edits.";
  });
  snapshotsHint.fontSize = 12.0f;
  snapshotsHint.color = kColorTextMuted;
  snapshotsHint.style.margin = EdgeInsets{0, 0, 6, 0};
  snapshotsPane.addChild(std::move(snapshotsHint));

  {
    TextInput ti;
    ti.placeholder = "Name this snapshot";
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
    ti.onChange = [](const std::string &s) { filterSnapshotNameField(s); };
    ti.onSubmit = [](const std::string &) {
      addSnapshot();
    }; // Enter saves, like the button
    uiState.names.snapshot = ti.state;
    snapshotsPane.addChild(std::move(ti));
    registerTextField(uiState.names.snapshot, snapshotsPane.children.back());
  }
  snapshotsPane.addChild(
      sidebarButton("Save snapshot", addSnapshot, {}, true, [] {
        return state.original != nullptr &&
               state.snapshots.size() < kMaxSnapshots;
      }));

  Text snapshotsEmpty;
  snapshotsEmpty.label =
      std::string("No snapshots yet. Name the current state and save it.");
  snapshotsEmpty.fontSize = 12.0f;
  snapshotsEmpty.color = kColorTextMuted;
  snapshotsEmpty.style.margin = EdgeInsets{6, 0, 0, 0};
  snapshotsEmpty.style.display = std::function<Display()>(
      [] { return state.snapshots.empty() ? Display::Flex : Display::None; });
  snapshotsPane.addChild(std::move(snapshotsEmpty));

  for (size_t i = 0; i < kMaxSnapshots; ++i)
    snapshotsPane.addChild(buildSnapshotRow(i));

  snapshotsPane.addChild(sidebarButton(
      "Delete selected snapshot", deleteSelectedSnapshot, {}, false, [] {
        return uiState.snapshots.sel >= 0 &&
               static_cast<size_t>(uiState.snapshots.sel) < state.snapshots.size();
      }));

  return snapshotsPane;
}
