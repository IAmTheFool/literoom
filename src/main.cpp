// Literoom - the photo editor: UI and glue around the engine headers below.

#define LITEUI_IMPLEMENTATION
#include "liteui.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

// Engine headers: pure logic with no UI, each covered by its own unit
// test in tests/ (see CMakeLists.txt). Grouped by folder, lowest layer first.

// core: shared vocabulary
#include "core/adjustments.hpp"
#include "core/auto_tone.hpp"
#include "core/geometry.hpp"
#include "core/image16_convert.hpp"
#include "core/pixel_math.hpp"
#include "core/raster_ops.hpp"
#include "core/wide_chain.hpp"

// color
#include "color/color_grade.hpp"
#include "color/color_mix.hpp"
#include "color/color_pass.hpp"
#include "color/curves.hpp"

// detail effects
#include "detail/detail_layer.hpp"
#include "detail/neighbor_fx.hpp"

// masks and object selection
#include "mask/mask.hpp"
#include "mask/sam_segmenter.hpp"
#include "mask/seg_basic.hpp"

// saving and files
#include "io/color_tag.hpp"
#include "io/exif.hpp"
#include "io/file_utils.hpp"
#include "io/image_codec16.hpp"
#include "io/image_meta.hpp"
#include "io/image_source.hpp"
#include "io/minijson.hpp"
#include "io/photo_meta.hpp"
#include "io/preset.hpp"
#include "io/sidecar_file.hpp"

// edit state and rendering
#include "edit/adjustment_set.hpp"
#include "render/histogram.hpp"
#include "render/render.hpp"
#include "render/scopes.hpp"

// background work
#include "workers/export_worker.hpp" 

#include "workers/lut_preview_worker.hpp"
#include "workers/thumb_worker.hpp"

#include "app/constants.hpp"
#include "app/workers.hpp" // every background worker, in one place
#include "app/editor_state.hpp"
#include "app/ui_state.hpp"
#include "app/canvas_view.hpp"
#include "app/render_update.hpp"
#include "app/crop.hpp"
#include "app/geometry_worker.hpp"
#include "app/crop_tool.hpp"
#include "app/rotate.hpp"
#include "app/segmentation.hpp"
#include "app/saved_edits.hpp"
#include "app/lut_pane.hpp"
#include "app/sync.hpp"
#include "app/export.hpp"
#include "app/filmstrip.hpp"
#include "app/meta_commands.hpp"
#include "app/widgets.hpp"
#include "app/color_ui.hpp"
#include "app/masks.hpp"
#include "app/presets.hpp"
#include "app/snapshots.hpp"
#include "app/virtual_copies.hpp"
#include "app/mask_list.hpp"
#include "app/mask_shapes.hpp"
#include "app/mask_polygon.hpp"
#include "app/mask_brush.hpp"
#include "app/mask_overlays.hpp"
#include "app/mask_widgets.hpp"
#include "app/history_panel.hpp"
#include "app/crop_overlay.hpp"
#include "app/compare.hpp"
#include "app/canvas_paint.hpp"
#include "app/readout.hpp"
#include "app/wb_picker.hpp"
#include "app/auto_tone_hist.hpp"
#include "app/curve_editor.hpp"
#include "app/mask_effects.hpp"
#include "app/mask_curve.hpp"
#include "app/startup_checks.hpp"
#include "app/panes/pane_helpers.hpp"
#include "app/panes/info_pane.hpp"
#include "app/panes/history_pane.hpp"
#include "app/panes/snapshots_pane.hpp"
#include "app/panes/adjust_pane.hpp"
#include "app/panes/presets_pane.hpp"
#include "app/panes/luts_pane.hpp"
#include "app/panes/curve_pane.hpp"
#include "app/panes/mix_pane.hpp"
#include "app/panes/grade_pane.hpp"
#include "app/panes/masks_pane.hpp"
#include "app/panes/crop_pane.hpp"
#include "app/panes/rotate_pane.hpp"
#include "app/panes/export_pane.hpp"
#include "app/shell/shortcuts.hpp"
#include "app/shell/titlebar.hpp"
#include "app/shell/activity_bar.hpp"
#include "app/shell/histogram_view.hpp"
#include "app/shell/sidebar.hpp"
#include "app/shell/sidebar_divider.hpp"
#include "app/shell/center_column.hpp"
#include "app/shell/mask_panel.hpp"
#include "app/shell/status_bar.hpp"

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
  flushSidecar();            // edits made since the last autosave
  workers().shutdown();      // join every worker thread before `ui` goes out of
                             // scope (a decode still running is waited out)
  return 0;
}