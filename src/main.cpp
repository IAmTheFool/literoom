// Literoom - the photo editor: UI and glue around the engine headers.
//
// Every header below includes what it uses, so this file lists only what
// main() itself touches; the rest arrives through them.

// LITEROOM_NO_LITEUI_IMPL: set only by tests/odr_second_tu.cpp, the second
// translation unit of the ODR check (liteui's implementation may be compiled into
// exactly one TU of a program).
// This define and the include after it must stay the FIRST thing in the file:
// liteui.hpp turns into the library's implementation only if the define is
// already visible the first time the header is included.
#ifndef LITEROOM_NO_LITEUI_IMPL
#define LITEUI_IMPLEMENTATION
#endif
#include "liteui.hpp"

#include <utility> // std::move
#include <vector>  // std::vector<Activity>

#include "app/canvas_view.hpp"    // setDpr
#include "app/constants.hpp"      // Activity, k*Activity ids, kColorRootBg
#include "app/editor_state.hpp"   // state.openMenu
#include "app/saved_edits.hpp"    // flushSidecar
#include "app/startup_checks.hpp" // verifyAdjustmentSpecs
#include "app/widgets.hpp"        // applyClickCursors
#include "app/workers.hpp"        // workers(), WorkersShutdownGuard

#include "app/shell/activity_bar.hpp"    // buildActivityBar
#include "app/shell/center_column.hpp"   // buildCenterColumn
#include "app/shell/mask_panel.hpp"      // addMaskPanel
#include "app/shell/shortcuts.hpp"       // installShortcuts
#include "app/shell/sidebar.hpp"         // buildSidebar
#include "app/shell/sidebar_divider.hpp" // buildSidebarDivider
#include "app/shell/status_bar.hpp"      // buildStatusBar
#include "app/shell/titlebar.hpp"        // buildTitlebar

int main() {
  verifyAdjustmentSpecs();
  LiteUI ui("Literoom", -1, -1, /*hideTitlebar=*/true);
  // Zoom is in device pixels (100% = one image pixel per physical pixel), so the
  // display scale must be known before the first layout and follow the window
  // between monitors. The handler runs before LiteUI relayouts and repaints.
  setDpr(ui.contentScale());
  ui.setScaleChangedHandler([&ui] { setDpr(ui.contentScale()); });
  // A finished background job wakes the (otherwise sleeping) event loop.
  workers().setNotify([&ui] { ui.wake(); });
  // Joins every worker thread when main() is left, before `ui` is destroyed.
  const WorkersShutdownGuard workersGuard;

  installShortcuts(ui);

  const std::vector<Activity> activities = {
      {kInfoActivity,        "Info",          "I"},
      {kHistoryActivity,     "History",       "H"},
      {kSnapshotsActivity,   "Snapshots",     "S"},
      {kAdjustActivity,      "Adjust",        "A"},
      {kPresetsActivity,     "Presets",       "P"},
      {kLutsActivity,        "LUTs",          "L"},
      {kCurveActivity,       "Curve",         "~"},
      {kColorMixActivity,    "Color Mix",     "M"},
      {kColorGradingActivity,"Color Grading", "G"},
      {kMasksActivity,       "Masks",         "\u25D0"},
      {kCropActivity,        "Crop",          "C"},
      {kRotateActivity,      "Rotate",        "R"},
      {kExportActivity,      "Export",        "E"},
  };

  View root;
  root.style.direction = FlexDirection::Column;
  root.style.width = Size::full();
  root.style.height = Size::full();
  root.style.backgroundColor = kColorRootBg;

  root.addChild(buildTitlebar(ui));

  // Workspace: activity bar, sidebar, divider, canvas viewport.

  View workspace;
  workspace.style.direction = FlexDirection::Row;
  workspace.style.width = Size::full();
  workspace.style.flexGrow = 1;
  // Clicking anywhere in the workspace that isn't itself a button
  // (e.g. the canvas, or blank sidebar space) closes an open menu too.
  workspace.onPointerDown = [](PointerEvent &e) {
    if (e.button == MouseButton::Left)
      state.openMenu = -1;
  };

  workspace.addChild(buildActivityBar(activities));

  // The selected mask's editor is built together with the Masks pane (in the
  // sidebar) but lives in the right-hand panel, added below.
  View maskEditor;
  workspace.addChild(buildSidebar(activities, maskEditor));
  workspace.addChild(buildSidebarDivider());
  workspace.addChild(buildCenterColumn());
  addMaskPanel(workspace, std::move(maskEditor));

  root.addChild(std::move(workspace));

  root.addChild(buildStatusBar());

  applyClickCursors(root);
  ui.setRoot(std::move(root));
  ui.run();
  flushSidecar(); // edits made since the last autosave
  // workersGuard joins the worker threads as main() returns, before `ui` is
  // destroyed (a decode still running is waited out).
  return 0;
}