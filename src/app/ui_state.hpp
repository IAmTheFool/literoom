// app/ui_state.hpp: UI-only state: tabs, selections, clipboards (`uiState`)
#pragma once

#include <memory>   // std::shared_ptr
#include <optional> // std::optional
#include <string>   // std::string
#include <vector>   // std::vector

#include "liteui.hpp" // TextInputState

#include "color/color_grade.hpp" // ColorGrade
#include "color/color_mix.hpp"   // MixChan, MixValues
#include "color/curves.hpp"      // CurveChannel, CurveSet, identityCurves
#include "core/adjustments.hpp"  // AdjValues
#include "io/preset.hpp"         // Preset

// ---------------- UI-only state ----------------
// What the panes remember that is neither an edit nor part of the picture:
// which tab is showing, which list row was clicked, the Copy Settings
// clipboard, the name fields' text state. The edits live in state.adjust and
// the picture in `state`; this is only how the UI is showing them. Touched by
// the UI thread only (workers never see it). One instance: `uiState`.

// One row of the Presets list: a preset file and what it holds.
struct PresetEntry {
  std::string name;
  std::string path;
  Preset preset;
};

// What Copy / Paste Effects moves between masks: everything but the shape.
struct MaskEffects {
  float amount = 100.0f;
  AdjValues adj{};
  MixValues mix{};
  ColorGrade grade;
  CurveSet curves = identityCurves();
};

struct UiState {
  // Color Mixer pane
  struct Mix {
    MixChan chan = MixChan::Hue; // which of Hue / Sat / Lum is shown
  } mix;

  // Color Grading pane
  struct Grade {
    int mode = 0;   // 0 = 3-way, 1..4 = Shadows..Global
    int active = 1; // wheel shown in the H/S/L readout (Midtones)
  } grade;

  // Tone curve editor (canvas side)
  struct Curve {
    int active = -1;                             // point being dragged, or -1
    CurveChannel channel = CurveChannel::Master; // curve being edited
    // Set by UI-only changes (channel switch, selecting a point) that don't
    // change the picture, so the curve canvas still repaints; consumed by its
    // dirty poll.
    bool uiDirty = false;
    // (point - mouse) in curve units at press, so a grabbed point doesn't jump
    // to the cursor's exact position.
    float grabDx = 0.0f, grabDy = 0.0f;
  } curve;

  // Masks pane, Effect tab (Mix / Grade / Curve)
  struct MaskPane {
    MixChan mixChan = MixChan::Hue;              // Mix: Hue / Sat / Lum
    int gradeRange = 1;                          // Grade: 0..3 (Midtones)
    CurveChannel curveCh = CurveChannel::Master; // Curve: channel
    int curveActive = -1;                        // point being dragged
    bool curveUiDirty = false;                   // repaint-only change
    float curveGrabDx = 0.0f, curveGrabDy = 0.0f;
  } maskPane;

  // Presets pane: the files in the presets folder (a cache, re-read when the
  // pane is opened and after a save or delete) and the last row clicked.
  struct Presets {
    std::vector<PresetEntry> list;
    int sel = -1; // the last preset clicked, or -1
  } presets;

  // Snapshots pane
  struct Snapshots {
    int sel = -1; // the last snapshot clicked, or -1
  } snapshots;

  // In-memory clipboards
  struct Clipboard {
    // Copy / Paste Settings: same payload as a preset (the look only), kept in
    // memory instead of a file, and where it was copied from.
    std::optional<Preset> settings;
    std::string settingsFrom;
    std::optional<MaskEffects> maskEffects; // Copy / Paste Effects
  } clipboard;

  // The TextInput of a name field keeps its text in a shared state; we hold a
  // pointer to it so the field can follow the selection (selection change,
  // undo / redo) and a command can read and clear it.
  struct Names {
    std::shared_ptr<TextInputState> mask;     // the Masks pane's name field
    std::shared_ptr<TextInputState> preset;   // the Presets pane's name field
    std::shared_ptr<TextInputState> snapshot; // the Snapshots pane's name field
  } names;

  // Width of the histogram canvas as last painted (for corner hit-testing).
  struct Hist {
    float canvasW = 0.0f;
  } hist;

  // Path of the copy "Delete Virtual Copy" was pressed for (press it twice).
  std::string vcDeleteArmed;

  // ---- Selections that point into the picture's edits ----
  // A curve editor's selected point is an index into a curve, and a snapshot
  // row is an index into the picture's snapshot list. Both stop meaning
  // anything when the edits they point into are replaced, so the code that
  // replaces them calls these (new picture, undo / redo / History jump, a
  // preset or snapshot, a different mask).

  // The Curve pane's selected point.
  void dropCurveSelection() {
    curve.active = -1;
    curve.grabDx = curve.grabDy = 0.0f;
    curve.uiDirty = true; // repaint without a picture change
  }
  // The Masks pane's Curve tab: its point belongs to the selected mask.
  void dropMaskCurveSelection() {
    maskPane.curveActive = -1;
    maskPane.curveGrabDx = maskPane.curveGrabDy = 0.0f;
    maskPane.curveUiDirty = true;
  }
  void dropCurveSelections() {
    dropCurveSelection();
    dropMaskCurveSelection();
  }
  // A different picture (or virtual copy) was opened: nothing selected for the
  // previous one applies. The Presets selection stays: presets are files, not
  // part of any picture.
  void resetForNewPicture() {
    dropCurveSelections();
    snapshots.sel = -1;
    vcDeleteArmed.clear();
  }
};
inline UiState uiState;
