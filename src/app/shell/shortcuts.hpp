// app/shell/shortcuts.hpp: keyboard shortcuts (installShortcuts)
#pragma once

#include <functional> // std::function
#include <utility>    // std::move

#include "liteui.hpp" // LiteUI, Key, KeyModifiers, KeyEvent

#include "app/auto_tone_hist.hpp" // autoTone, toggleClipping, toggleChannelClipping
#include "app/canvas_view.hpp"  // setZoom
#include "app/compare.hpp"      // toggleBefore, toggleSplit
#include "app/constants.hpp"    // kZoomStep
#include "app/crop.hpp"         // cropToolActive
#include "app/crop_tool.hpp"    // applyCrop
#include "app/editor_state.hpp" // inline EditorState state, clearStatus
#include "app/export.hpp"       // exportSelected
#include "app/filmstrip.hpp" // filmSelectAll, filmStep, toggleFilmstrip, openFolder
#include "app/masks.hpp"     // maskToolActive, typingInTextField
#include "app/meta_commands.hpp" // openImage, exportImage, toggleSidebar, ratePhoto,
                                 // flagPhoto, labelPhoto
#include "app/presets.hpp"       // copySettings, pasteSettings
#include "app/rotate.hpp"        // undo, redo, applyGeoOp
#include "app/sync.hpp"          // syncSettings
#include "app/virtual_copies.hpp" // createVirtualCopy, deleteSelectedMask
#include "app/wb_picker.hpp"      // toggleWbPicker
#include "app/widgets.hpp"        // forwardKeyToTextField
#include "core/geometry.hpp"      // GeoOp
#include "io/photo_meta.hpp"      // PhotoFlag, kMaxRating

// ---- Keyboard shortcuts ----
// LiteUI's addShortcut is global: it fires whatever has focus, so these work
// after clicking a sidebar button too. Modifiers must match exactly.
// Zoom uses the unshifted Ctrl+= / Ctrl+-: on Linux LiteUI reports shifted
// symbols like '+' as Key::Unknown, so Ctrl+Shift+= can't be bound there.
inline void installShortcuts(LiteUI &ui) {
  auto withMods = [](bool ctrl, bool shift) {
    KeyModifiers m;
    m.ctrl = ctrl;
    m.shift = shift;
    return m;
  };
  const KeyModifiers none = withMods(false, false);
  const KeyModifiers shiftOnly = withMods(false, true);
  const KeyModifiers ctrl = withMods(true, false);
  const KeyModifiers ctrlShift = withMods(true, true);
  // Edits and dialogs are ignored mid-drag (a slider or crop handle is
  // being held): changing the image under the drag would fight it.
  auto idle = [](std::function<void()> fn) {
    return [fn = std::move(fn)] {
      if (!state.dragging)
        fn();
    };
  };
  // Plain-key shortcuts must not fire while the mask name field is being
  // typed in (see typingInTextField).
  auto notTyping = [idle](std::function<void()> fn) {
    return idle([fn = std::move(fn)] {
      if (!typingInTextField())
        fn();
    });
  };
  // Left / Right / Delete / Enter are also the keys a text field edits with.
  // While one has focus it gets the key (see forwardKeyToTextField);
  // otherwise the shortcut runs.
  auto fieldFirst = [none](Key key, std::function<void()> fn) {
    return [key, none, fn = std::move(fn)] {
      if (!forwardKeyToTextField(KeyEvent{key, none}))
        fn();
    };
  };
  auto zoomIn = [] { setZoom(state.zoomDev * kZoomStep); };
  auto zoomOut = [] { setZoom(state.zoomDev / kZoomStep); };

  ui.addShortcut(ctrlShift, Key::C, notTyping(copySettings));
  ui.addShortcut(ctrlShift, Key::V, notTyping(idle(pasteSettings)));

  ui.addShortcut(ctrl, Key::Z, idle(undo));
  ui.addShortcut(ctrl, Key::Y, idle(redo));
  ui.addShortcut(ctrlShift, Key::Z, idle(redo));
  ui.addShortcut(ctrl, Key::O, idle(openImage));
  ui.addShortcut(ctrlShift, Key::S, notTyping(idle(syncSettings)));
  ui.addShortcut(ctrl, Key::A, notTyping(filmSelectAll));
  ui.addShortcut(ctrl, Key::E, idle(exportImage));
  ui.addShortcut(ctrlShift, Key::E, notTyping(idle(exportSelected)));
  ui.addShortcut(ctrl, Key::Q, [&ui] { ui.requestClose(); });
  ui.addShortcut(ctrl, Key::B, toggleSidebar);
  ui.addShortcut(ctrl, Key::Equal, zoomIn);
  ui.addShortcut(ctrlShift, Key::Equal,
                 zoomIn); // '+' on layouts that report it
  ui.addShortcut(ctrl, Key::Minus, zoomOut);
  ui.addShortcut(ctrl, Key::N0, [] { setZoom(1.0f); });
  ui.addShortcut(ctrl, Key::LeftBracket,
                 idle([] { applyGeoOp(GeoOp::RotateCCW); }));
  ui.addShortcut(ctrl, Key::RightBracket,
                 idle([] { applyGeoOp(GeoOp::RotateCW); }));
  ui.addShortcut(ctrlShift, Key::O, idle(openFolder));
  ui.addShortcut(
      ctrlShift, Key::N,
      notTyping(idle(createVirtualCopy))); // listed in the Image menu
  ui.addShortcut(none, Key::Left,
                 fieldFirst(Key::Left, notTyping([] { filmStep(-1); })));
  ui.addShortcut(none, Key::Right,
                 fieldFirst(Key::Right, notTyping([] { filmStep(1); })));
  ui.addShortcut(none, Key::F6, toggleFilmstrip);
  ui.addShortcut(none, Key::Backslash,
                 notTyping(toggleBefore));              // Before / After
  ui.addShortcut(none, Key::Y, notTyping(toggleSplit)); // split view
  ui.addShortcut(none, Key::J,
                 notTyping(toggleClipping)); // clipping warnings
  ui.addShortcut(shiftOnly, Key::J,
                 notTyping(toggleChannelClipping)); // per-channel colors
  ui.addShortcut(none, Key::W,
                 notTyping(toggleWbPicker));         // white-balance picker
  ui.addShortcut(none, Key::A, notTyping(autoTone)); // Auto Tone
  // Rating (0 clears, 1-5), flag (P pick, X reject, U unflag) and color label
  // (6 red, 7 yellow, 8 green, 9 blue) of the open picture. Not edits.
  for (int s = 0; s <= kMaxRating; ++s)
    ui.addShortcut(none, static_cast<Key>(static_cast<int>(Key::N0) + s),
                   notTyping([s] { ratePhoto(s); }));
  ui.addShortcut(none, Key::P, notTyping([] { flagPhoto(PhotoFlag::Pick); }));
  ui.addShortcut(none, Key::X, notTyping([] { flagPhoto(PhotoFlag::Reject); }));
  ui.addShortcut(none, Key::U, notTyping([] { flagPhoto(PhotoFlag::None); }));
  for (int c = 1; c <= 4; ++c)
    ui.addShortcut(none, static_cast<Key>(static_cast<int>(Key::N0) + 5 + c),
                   notTyping([c] { labelPhoto(c); }));
  ui.addShortcut(none, Key::Escape, [] {
    state.openMenu = -1;
    if (state.wbPicking) {
      state.wbPicking = false; // cancel the picker
      clearStatus();
      state.markDirty();
    }
  });
  ui.addShortcut(none, Key::Delete, fieldFirst(Key::Delete, notTyping([] {
                                                 if (maskToolActive())
                                                   deleteSelectedMask();
                                               })));
  ui.addShortcut(
      none, Key::Enter,
      fieldFirst(Key::Enter, idle([] {
                   if (cropToolActive())
                     applyCrop(); // commit the selection, like the button
                 })));
}
