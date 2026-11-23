# App headers

Reference for every header under `src/app/`, the UI shell of Literoom: the code that used to be one 11,600-line `main.cpp`. It is the counterpart of [headers.md](headers.md), which documents the **engine** headers (`core/`, `color/`, `io/`, `mask/`, `render/`, `workers/`, ...). The engine is pure, tested, and knows nothing about a window; everything here is the part that does: it owns the editor state, builds the views, handles the pointer and keyboard, and drives the background workers. For the big picture (layers, threads, undo) read [architecture.md](architecture.md) and [threading.md](threading.md) first.

## How these files fit together

- **One translation unit.** Every file is a `#pragma once` header that only `main.cpp` includes, in the fixed order of the [include order](#include-order) table. They are **not self-contained**: each one uses what the files before it define, and a few functions are forward-declared (listed in the entry that declares them). `tests/golden_render_test.cpp` and `tests/lut_pane_test.cpp` compile `main.cpp` itself, so they pick these files up through it.
- **What is left in `main.cpp`:** the program's includes, `int main()` and nothing else. `main()` creates the window, wires every worker's notify callback to `LiteUI::wake`, calls `installShortcuts`, assembles the pieces of the [window shell](#window-shell-shell) in order, runs the event loop and shuts the workers down.
- **Two shared objects.** `state` (`EditorState`, [editor_state.hpp](#editor_statehpp)) is the editor's model: the picture at every stage, the edits (`state.adjust`), the view, the filmstrip, the crop and mask tool state. `uiState` (`UiState`, [ui_state.hpp](#ui_statehpp)) is the UI-only part: selected tabs, list selections, clipboards. Almost every file reads and writes them, so the **Depends on** lines below leave out `editor_state.hpp`, `ui_state.hpp` and `constants.hpp`.
- **Pane code is built once.** The view tree is created at startup. A pane is always in the tree and shows itself only while its activity is selected, through style and label lambdas that LiteUI re-evaluates after every event; lists (masks, presets, LUTs, snapshots, history) are a fixed number of row slots that each show themselves only for an index that exists. Nothing rebuilds views at run time.

## Threads

Only these run off the UI thread, and each is a `LatestWorker` (one lazily started thread; a new request supersedes an older one; the finished result is parked until the UI thread collects it): `renderWorker` (`render_worker.hpp`), `geometryWorker` (`geometry_worker.hpp`), `decodeWorker` (`decode.hpp`), `segmentWorker` (`segmentation.hpp`), `syncWorker` (`sync.hpp`), `exportWorker` (`export.hpp`), `thumbWorker` (`filmstrip.hpp`) and `lutPreviewWorker` (`lut_pane.hpp`). Functions marked **Worker thread only** in the entries are the only code that runs there; they must not read or write `state` or `uiState` and must not call LiteUI except `wake()`. A worker announces a result by waking the event loop; the loop then polls, and every `collect...` function (`collectFullRender`, `collectGeometryJob`, `collectDecodeJob`, `collectSegmentJob`, `collectSyncJob`, `collectExportJob`, plus `pollLutPreviews` and `collectThumbJobs`) takes the result in on the UI thread. They are all called from one place, `flushPendingAdjust()` ([auto_tone_hist.hpp](#auto_tone_histhpp)), which is itself polled once per frame by the main canvas and the histogram canvas.

## Words used throughout

- **Document pixels / image pixels / canvas-local.** The picture on screen is `state.docWidth() × docHeight()` document pixels (the oriented, straightened, cropped picture). Pointer handlers receive **canvas-local** logical units; `viewLocal` subtracts the picture's corner, and dividing by `state.zoom` gives document pixels. Masks are *stored* as fractions of the **original** picture and converted to the in-view form through a `GeoFrame` (`shapeInView`).
- **Preview path / deferred recompute.** While a slider, curve point, mask handle or wheel is being dragged, the editor renders a small preview (`previewBase` → `previewImage`) instead of the full picture (`enterDrag`, `updatePreviewFromAdjustments`). A change marks the picture stale with `state.markAdjustDirty()`, and `flushPendingAdjust()` recomputes **once per frame** however many pointer events arrived. When the drag ends, one full-resolution render replaces the preview.
- **Undo step / undo key.** Every edit goes through `AdjustmentSet`, which records one undo step per change; an edit with a non-negative **key** that repeats the previous key is folded into the same step (a drag is one step). Mask edits build their key from the mask, the part and a `kMaskCtl*` control id (see [masks.hpp](#maskshpp)).
- **Stand-in.** The picture shown while a background result is awaited: `state.previewImage` until the full-resolution render, or the previously open picture until a decode finishes.

## How an entry is laid out

Each entry follows [headers.md](headers.md): a short description, a **Depends on / Test** line, then, where the file has them, **Constants** (tables with type, value and meaning), **Types** (fields with type, default and meaning; methods; nested types), **State and tables** (file-scope variables), **Forward declarations**, and **Functions**: the signature exactly as in the source, then what it does: inputs, return value and the edge cases that matter. Descriptions come from reading the code; where the source has its own comment it is kept.

- **Depends on** is computed from the identifiers a file uses: other `app/` files (marked *defined later* when the function is forward-declared and defined after this file), and engine headers (from the names documented in [headers.md](headers.md)). It is a guide to where to look, not a build-order contract; the [include order](#include-order) is.
- **Test:** the unit tests in `tests/cases/` cover the engine. Nothing in `src/app/` has a case of its own except what `tests/lut_pane_test.cpp` drives for the LUTs pane; the golden render test and the LUT-pane test compile all of it through `main.cpp`.

## Contents

- **Foundations**: [`constants.hpp`](#constantshpp), [`render_worker.hpp`](#render_workerhpp), [`editor_state.hpp`](#editor_statehpp), [`ui_state.hpp`](#ui_statehpp), [`canvas_view.hpp`](#canvas_viewhpp), [`render_update.hpp`](#render_updatehpp)
- **Crop, geometry and rotation**: [`crop.hpp`](#crophpp), [`geometry_worker.hpp`](#geometry_workerhpp), [`crop_tool.hpp`](#crop_toolhpp), [`rotate.hpp`](#rotatehpp)
- **Opening, segmentation and saving**: [`decode.hpp`](#decodehpp), [`segmentation.hpp`](#segmentationhpp), [`saved_edits.hpp`](#saved_editshpp)
- **LUTs, Sync Settings and batch export**: [`lut_pane.hpp`](#lut_panehpp), [`sync.hpp`](#synchpp), [`export.hpp`](#exporthpp)
- **Filmstrip and per-picture commands**: [`filmstrip.hpp`](#filmstriphpp), [`meta_commands.hpp`](#meta_commandshpp)
- **Widgets and colour controls**: [`widgets.hpp`](#widgetshpp), [`color_ui.hpp`](#color_uihpp)
- **Masks, presets, snapshots and virtual copies**: [`masks.hpp`](#maskshpp), [`presets.hpp`](#presetshpp), [`snapshots.hpp`](#snapshotshpp), [`virtual_copies.hpp`](#virtual_copieshpp), [`mask_list.hpp`](#mask_listhpp), [`mask_shapes.hpp`](#mask_shapeshpp), [`mask_polygon.hpp`](#mask_polygonhpp), [`mask_brush.hpp`](#mask_brushhpp), [`mask_overlays.hpp`](#mask_overlayshpp), [`mask_widgets.hpp`](#mask_widgetshpp), [`history_panel.hpp`](#history_panelhpp)
- **Canvas painting, picture tools and the curve editors**: [`crop_overlay.hpp`](#crop_overlayhpp), [`compare.hpp`](#comparehpp), [`canvas_paint.hpp`](#canvas_painthpp), [`readout.hpp`](#readouthpp), [`wb_picker.hpp`](#wb_pickerhpp), [`auto_tone_hist.hpp`](#auto_tone_histhpp), [`curve_editor.hpp`](#curve_editorhpp), [`mask_effects.hpp`](#mask_effectshpp), [`mask_curve.hpp`](#mask_curvehpp), [`startup_checks.hpp`](#startup_checkshpp)
- **Panes (`panes/`)**: [`panes/pane_helpers.hpp`](#panespane_helpershpp), [`panes/info_pane.hpp`](#panesinfo_panehpp), [`panes/history_pane.hpp`](#paneshistory_panehpp), [`panes/snapshots_pane.hpp`](#panessnapshots_panehpp), [`panes/adjust_pane.hpp`](#panesadjust_panehpp), [`panes/presets_pane.hpp`](#panespresets_panehpp), [`panes/luts_pane.hpp`](#panesluts_panehpp), [`panes/curve_pane.hpp`](#panescurve_panehpp), [`panes/mix_pane.hpp`](#panesmix_panehpp), [`panes/grade_pane.hpp`](#panesgrade_panehpp), [`panes/masks_pane.hpp`](#panesmasks_panehpp), [`panes/crop_pane.hpp`](#panescrop_panehpp), [`panes/rotate_pane.hpp`](#panesrotate_panehpp), [`panes/export_pane.hpp`](#panesexport_panehpp)
- **Window shell (`shell/`)**: [`shell/shortcuts.hpp`](#shellshortcutshpp), [`shell/titlebar.hpp`](#shelltitlebarhpp), [`shell/activity_bar.hpp`](#shellactivity_barhpp), [`shell/histogram_view.hpp`](#shellhistogram_viewhpp), [`shell/sidebar.hpp`](#shellsidebarhpp), [`shell/sidebar_divider.hpp`](#shellsidebar_dividerhpp), [`shell/center_column.hpp`](#shellcenter_columnhpp), [`shell/mask_panel.hpp`](#shellmask_panelhpp), [`shell/status_bar.hpp`](#shellstatus_barhpp)
- **Appendices**: [Include order](#include-order), [Global variables and tables](#global-variables-and-tables), [Function index](#function-index)

## Foundations

### `constants.hpp`
Every fixed number and color the UI uses, in one place: layout sizes (title bar, status bar, activity bar, sidebar and mask-panel widths), the zoom range, the crop / curve / histogram / mask-overlay sizes, the activity ids and row counts of the list panes, and the dark palette with its semantic names. Plus the `Activity` struct and the display color of each Color Mixer band. Nothing here changes at run time. The engine's own constants live next to their algorithms (`color_mix.hpp`, `histogram.hpp`, `mask_geom.hpp`, ...); this file holds only what the window needs.

**Depends on:** engine: `color_mix.hpp`, `lut_library.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kTitlebarH` | `float` | `36.0f` | Title bar height |
| `kStatusBarH` | `float` | `26.0f` | Status bar height |
| `kActivityBarW` | `float` | `48.0f` | Activity bar (icon column) width |
| `kDividerW` | `float` | `4.0f` | Width of the draggable dividers |
| `kMinSidebarW` | `float` | `180.0f` | Narrowest the sidebar can be dragged |
| `kMaxSidebarW` | `float` | `420.0f` | Widest the sidebar can be dragged |

Right-hand panel of the Masks pane (the selected mask's controls).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kMinMaskPanelW` | `float` | `300.0f` | Narrowest the mask panel can be dragged |
| `kMaxMaskPanelW` | `float` | `480.0f` | Widest the mask panel can be dragged |
| `kMaskPanelPad` | `float` | `12.0f` | panel padding, each side |
| `kMaskTrackW` | `float` | `240.0f` | slider track length in that panel (kMinMaskPanelW - 2 * pad - scrollbar must stay >= this) |

Zoom range/step. Zoom is measured in DEVICE pixels (see EditorState::zoomDev): 1.0 = one image pixel per physical screen pixel, on any display scale.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kZoomMin` | `float` | `0.1f` | Smallest device zoom (10%) |
| `kZoomMax` | `float` | `8.0f` | Largest device zoom (800%) |
| `kZoomStep` | `float` | `1.25f` | Factor of one zoom-in / zoom-out step (menu, keys, buttons) |

Crop tool. Sizes marked "screen px" are constant on screen regardless of zoom (the overlay divides by state.zoom when drawing/hit-testing).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kCropMinPx` | `float` | `16.0f` | smallest selection edge, image px |
| `kCropHandlePx` | `float` | `8.0f` | drawn handle size, screen px |
| `kCropHitPx` | `float` | `10.0f` | handle grab tolerance, screen px |

Color Mixer tab width (the engine constants live in color_mix.hpp).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kMixTabW` | `float` | `46.0f` | Hue/Sat/Lum tab width (3 + gaps = 150) |

Histogram canvas height (the counting constants live in histogram.hpp).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kHistHeightPx` | `float` | `80.0f` | Height of the pinned histogram canvas |

The same canvas is taller while it shows a waveform or a vectorscope (the vectorscope is a square, so it needs the room). See scopes.hpp.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kScopeHeightPx` | `float` | `120.0f` | Height of the pinned canvas while it shows a waveform or vectorscope |

Clipping warnings: corner triangles on the histogram (the canvas overlay's constants live in histogram.hpp too).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kHistTriPx` | `float` | `9.0f` | triangle leg length |
| `kHistTriHitPx` | `float` | `18.0f` | click zone in each top corner |

Tone curve editor (canvas is square, kCurveSizePx on a side; the plot area is inset by kCurvePadPx so the end points aren't clipped).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kCurveSizePx` | `float` | `180.0f` | Side of the Curve pane's square canvas (the plot is this minus the inset on both sides) |
| `kCurvePadPx` | `float` | `8.0f` | Inset of the plot area inside the curve canvas |
| `kCurveHitPx` | `float` | `10.0f` | point grab tolerance, screen px |
| `kCurvePointPx` | `float` | `8.0f` | drawn point size, screen px |
| `kCurveTabW` | `float` | `42.0f` | channel tab width (4 tabs + gaps = canvas width) |

Document size before any image has been opened, so there's a visible page to scroll/zoom around from the start.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kPlaceholderW` | `float` | `800.0f` | Width of the stand-in page used for the document size before a picture is open (`docWidth()` fallback) |
| `kPlaceholderH` | `float` | `600.0f` | Height of that stand-in page |

Activity id of the History panel (see the activities list in main).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kHistoryActivity` | `int` | `9` | Activity id of the History pane (0..8 are the original panes) |

Activity id of the Presets panel, and how many preset rows it can show (the view tree is built once, like the mask and history lists).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kPresetsActivity` | `int` | `10` | Activity id of the Presets pane |
| `kPresetRows` | `size_t` | `24` | Rows built in the Presets list (at most this many presets are listed) |

Activity id of the Snapshots panel. It has kMaxSnapshots rows (sidecar.hpp); the view tree is built once, like the preset and history lists.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kSnapshotsActivity` | `int` | `11` | Activity id of the Snapshots pane |

Activity id of the LUTs panel. Like the preset list, its view tree is built once, with a row for every LUT the library can hold (kLutMaxFiles).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kLutsActivity` | `int` | `12` | Activity id of the LUTs pane |
| `kLutRows` | `size_t` | `kLutMaxFiles` | Rows built in the LUTs list (the library's file limit) |
| `kLutRowH` | `float` | `kFilmThumbH + 12.0f` | a row with its preview of the picture |
| `kLutRowNamesH` | `float` | `28.0f` | a row with no picture open: just the name |

Masks, UI side: tint-bitmap sizes and overlay handle sizes. The engine constants live next to their algorithms: mask_geom.hpp (radii), mask_brush.hpp, mask_range.hpp, mask_object.hpp, seg_basic.hpp and mask.hpp (list limits).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kCompTintEdge` | `int` | `512` | long side of a combined mask's tint |
| `kMaskHandlePx` | `float` | `9.0f` | overlay handle size, screen px |
| `kMaskHitPx` | `float` | `12.0f` | overlay handle grab tolerance, px |
| `kBrushTintEdge` | `int` | `512` | long side of the brush tint bitmap |
| `kLumTintEdge` | `int` | `512` | long side of the on-canvas tint bitmap |

Base swatches.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kPanelBg` | `Color` | `68, 68, 68` | Sidebar panel background |
| `kHeaderBg` | `Color` | `40, 40, 40` | Title bar, activity bar, status bar and window background |
| `kTitle` | `Color` | `176, 176, 176` | Title text |
| `kLabel` | `Color` | `160, 160, 160` | Muted label text |
| `kValue` | `Color` | `232, 232, 232` | Bright text (values, menu labels) |
| `kDivider` | `Color` | `46, 46, 46` | Divider background |
| `kTrackFill` | `Color` | `32, 32, 32` | Slider and scrollbar track |
| `kTrackEdge` | `Color` | `104, 104, 104` | Slider track edge; scrollbar thumb; the hairline around the picture |
| `kThumbFill` | `Color` | `196, 196, 196` | Slider thumb fill |
| `kThumbEdge` | `Color` | `28, 28, 28` | Slider thumb edge; dropdown border |
| `kIconRing` | `Color` | `205, 205, 205` | Icon colour (activity bar) |
| `kIconActiveBg` | `Color` | `92, 92, 92` | Selected activity button; dropdown row hover and "selected" background |
| `kTransparent` | `Color` | `0, 0, 0, 0` | Fully transparent |

Semantic names used throughout the UI, mapped onto the swatches above.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorTransparent` | `Color` | `kTransparent` | Alias of `kTransparent` |
| `kColorWhite` | `Color` | `255, 255, 255, 255` | real white (image, glyphs) |

Chrome backgrounds.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorRootBg` | `Color` | `kHeaderBg` | Window background (= `kHeaderBg`) |
| `kColorPanelBg` | `Color` | `kPanelBg` | sidebar |
| `kColorTitlebarBg` | `Color` | `kHeaderBg` | titlebar |
| `kColorActivityBarBg` | `Color` | `kHeaderBg` | activity bar, idle buttons |
| `kColorMenuHover` | `Color` | `62, 62, 62, 255` | menu / win-btn hover |
| `kColorActivityHover` | `Color` | `84, 84, 84, 255` | button / tab hover |
| `kColorActivityActive` | `Color` | `kIconActiveBg` | selected activity btn |
| `kColorDivider` | `Color` | `kDivider` | divider bg |
| `kColorTrack` | `Color` | `kTrackFill` | slider + scroll track |
| `kColorScrollThumb` | `Color` | `kTrackEdge` | Scrollbar thumb colour |

Text.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorTextDark` | `Color` | `kValue` | titles, menu labels |
| `kColorTextMid` | `Color` | `kTitle` | info pane, win-btn glyphs |
| `kColorTextIcon` | `Color` | `kIconRing` | activity bar icons |
| `kColorTextMuted` | `Color` | `kLabel` | section titles |
| `kColorTextFaint` | `Color` | `120, 120, 120, 255` | placeholders |

Dropdown menu.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorDropdownBg` | `Color` | `kHeaderBg` | Dropdown menu background |
| `kColorDropdownBorder` | `Color` | `kThumbEdge` | Dropdown menu border |
| `kColorDropdownRowHover` | `Color` | `kIconActiveBg` | also "selected" |

Window controls / accents.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorCloseHover` | `Color` | `232, 17, 35, 255` | Window close button on hover (red) |
| `kColorDividerHover` | `Color` | `100, 140, 220, 255` | accent (drag, fills) |
| `kColorAccent` | `Color` | `0, 122, 204, 255` | primary button |
| `kColorAccentHover` | `Color` | `30, 140, 220, 255` | Primary (accent) button on hover |

Canvas / viewport.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorCanvasPlaceholderBg` | `Color` | `52, 52, 52, 255` | Background of the canvas when no picture is open |
| `kColorViewportBg` | `Color` | `30, 30, 30, 255` | Background around the picture inside the canvas |
| `kColorCanvasBorder` | `Color` | `kTrackEdge` | The one-device-pixel hairline just outside the picture |
| `kColorEmptyCorner` | `Color` | `50, 50, 50, 255` | behind transparent corners of a straightened image |

Status bar.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kColorStatusBarBg` | `Color` | `kHeaderBg` | Status bar background |
| `kColorStatusHover` | `Color` | `kPanelBg` | Status-bar buttons on hover |
| `kColorStatusError` | `Color` | `255, 214, 102, 255` | Status-bar text colour for an error message |

**Types**

- `struct Activity`: A sidebar tab in the activity bar.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `id` | `int` |  |  |
  | `title` | `std::string` |  |  |
  | `icon` | `std::string` |  | single glyph |


**Functions**

**`static Color mixBandColor(size_t band)`**: **Input:** a Color Mixer band index `0..kMixBandCount-1` (`kMixBandCount` is in `color_mix.hpp`). **Returns:** the swatch shown on that band's tab and slider, in order red, orange, yellow, green, aqua, blue, purple, magenta. UI only; the math is in `color_mix.hpp`. No bounds check: an out-of-range band reads past the array.

### `render_worker.hpp`
The background render worker for the full-resolution pixel pass, so releasing a slider on a 24-megapixel photo doesn't freeze the window. The thread, the "latest wins" generation protocol and the parking slot are all in the engine's `latest_worker.hpp` (`LatestWorker`, tested on its own); this file only defines the rendering job and instantiates the worker. It is the one fenced file whose `// BEGIN renderworker` / `// END renderworker` markers are kept.

**Depends on:** engine: `image16.hpp`, `latest_worker.hpp`, `render_core.hpp`, `render_params.hpp`. **Test:** the worker protocol is the engine's `LatestWorker`, tested in `tests/cases/latest_worker.inc`; this file's own code has no case of its own. It is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `using RenderWorker = LatestWorker<RenderJob, RenderResult>`: `LatestWorker<RenderJob, RenderResult>`. The `src` of a submitted job must never be modified afterwards: the editor's source images are replaced, never edited in place.
- `struct RenderJob`: One render request: the immutable 8-bit source, the `RenderParams` to apply, and optionally the 16-bit twin of the source.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `src` | `std::shared_ptr<const CanvasImage>` |  |  |
  | `params` | `RenderParams` |  |  |
  | `src16` | `std::shared_ptr<const Image16>` |  | The 16-bit twin of `src` (EditorState::source16), if the picture has one: the render starts from it and reads it for the neighbor effects too; `src` is then only its 8-bit view (what the tools measure). |

- `struct RenderResult`: The finished, display-ready image.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `image` | `std::shared_ptr<CanvasImage>` |  |  |


**State and tables**

- `renderWorker`: `static RenderWorker`, initially `{runRenderJob}`

**Functions**

**`static std::optional<RenderResult> runRenderJob(RenderJob &&job, WorkerCancel c)`**: **Worker thread only.** Runs one `RenderJob`: from the 16-bit twin with `renderToDisplay` when `job.src16` is set, otherwise `renderWith` on the 8-bit `job.src`. Both poll `c.latest` / `c.gen`, so a superseded render abandons itself within milliseconds. **Returns** `std::nullopt` if it was abandoned part-way (nothing is parked or announced), otherwise a `RenderResult` holding the finished image. Touches no `state` and no LiteUI call.

### `editor_state.hpp`
The editor's one big mutable model, `EditorState`, and its single instance `state` (declared at the end of the file), plus the small state structs it holds: the live crop selection (`CropUi`), the mask-overlay drag and caches (`MaskUi`), the filmstrip (`FilmEntry`, `FilmStrip`) and the Before / After mode (`CompareMode`). It also holds the status-bar message helpers and the one function that sets the zoom. Everything here is **UI thread only**; no worker may read or write it (see [threading.md](threading.md)). Edits themselves are not stored as loose fields: they live in `state.adjust` (an `AdjustmentSet`, so they are undoable). The picture goes through a chain of stages, each kept so that tools and drags don't have to recompute earlier ones: `original` (decoded) → `oriented` (rotated / flipped) → `rotated` (straightened) → `source` (committed crop applied; what adjustments are run on) → `image` (adjustments applied; what is drawn and exported). A 16-bit PNG also keeps a 16-bit twin of each stage.

**Depends on:** `render_worker.hpp`, `masks.hpp` (defined later); engine: `color_tag.hpp`, `exif.hpp`, `geometry.hpp`, `histogram.hpp`, `image16.hpp`, `mask.hpp`, `mask_brush.hpp`, `mask_object.hpp`, `mask_polygon.hpp`, `mask_range.hpp`, `photo_meta.hpp`, `scopes.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Reading guide for `EditorState`**
- **Picture stages:** `original`, `oriented` (+`orientedFor`, `orientedPreview`), `rotated` (+`rotatedAngle`, `rotatedExpand`), `source` (+`sourceCrop`), `image`; the 16-bit twins `original16`, `oriented16`, `rotated16`, `source16`.
- **View:** `zoom`, `zoomDev`, `dpr`, and `view` (canvas size, scroll, scrollbar drag). Change them only through `applyZoomDev()` / `setZoom()` / `setDpr()`.
- **Edits and saved extras:** `adjust` (values + undo history), `sidecarSaved` (the `adjust.version()` at the last sidecar save or load), `meta` / `metaDirty`, `snapshots` / `snapshotsDirty`, `sidecarBlocked` (the picture's saved-edits file exists but could not be read, so nothing may overwrite it).
- **Drag-preview path:** `previewBase`, `previewBase16`, `previewImage`, `dragging`, `dragIsAngle`, `previewDocW/H`, `adjustDirty`.
- **Panes:** `crop`, `mask` + `maskSel` / `maskShowTint` / `maskTintColor` / `maskTab` / `maskFx` / `maskPanelW`, `film`, `compare` / `splitPos`, Export settings (`exportJpeg` ... `exportOverwrite`), `activeActivity`, `sidebarWidth`, `openMenu`, `maximized`.
- **Measuring tools:** histogram arrays and `histStale` / `histVersion`, `scopeKind` / `scopeImage`, clipping (`clipShadowsOn`, `clipHighlightsOn`, `histClipLo/Hi`, `clipPerChannel`, `clipOverlay`), the RGB readout (`hoverValid`, `hoverR/G/B`), `wbPicking`.
- **Housekeeping:** `statusMsg` / `statusIsError`, `imageSerial` (bumps on every opened file; keys the segmenter's caches), `imagePath`, `photoInfo`, `colorTag` and their serials.

**Constants**

Selection-drag "handle" bitmask: which edges of the selection a drag is moving. Two bits (e.g. L|T) = a corner; one bit = an edge; kEdgeMove = the whole rectangle.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kEdgeL` | `int` | `1` | Bit: left edge |
| `kEdgeR` | `int` | `2` | Bit: right edge |
| `kEdgeT` | `int` | `4` | Bit: top edge |
| `kEdgeB` | `int` | `8` | Bit: bottom edge |
| `kEdgeMove` | `int` | `16` | Not an edge: the interior of the selection (a drag moves it) |

**Types**

- `struct CropAspect`: Aspect-ratio presets in the Crop pane. ratio (width / height): 0 = free, negative = "same as the image being cropped".

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `label` | `const char *` |  |  |
  | `ratio` | `float` |  |  |

- `struct CropUi`: The live (not yet applied) selection rectangle and the drag currently editing it. Coordinates are in pixels of the image being shown (EditorState::image), not the original.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `l` | `float` | `0` |  |
  | `t` | `float` | `0` |  |
  | `r` | `float` | `0` |  |
  | `b` | `float` | `0` |  |
  | `aspectIdx` | `size_t` | `0` | index into kCropAspects |
  | `drag` | `enum class Drag` | `Drag::None` | Values: `None`, `New`, `Move`, `Resize`. |
  | `mask` | `int` | `0` | Resize: which edges (kEdge* bits) |
  | `ax` | `float` | `0` | New: the fixed anchor corner |
  | `ay` | `float` | `0` |  |
  | `grabX` | `float` | `0` | Move: pointer offset from selection's l/t |
  | `grabY` | `float` | `0` |  |

- `struct MaskUi`: Mask-overlay drag in progress (see maskPress / maskDrag).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `drag` | `enum class Drag` | `Drag::None` | Values: `None`, `Move`, `ResizeX`, `ResizeY`, `LinEnd`, `LinStart`, `Paint` (a brush stroke is being painted), `PolyVertex` (a polygon vertex is being dragged (see polyIdx)), `PolyMove` (the whole polygon is being dragged), `Lasso` (a freehand lasso is being drawn). |
  | `grabX` | `float` | `0` | Move: pointer offset from the mask's center |
  | `grabY` | `float` | `0` |  |
  | `brushSize` | `float` | `kBrushDefaultSize` | Brush tool. These settings apply to the NEXT stroke; every stroke keeps the values it was painted with. Size is a radius, as a fraction of the SHORTER side of the picture on screen. |
  | `brushFeather` | `float` | `0.5f` |  |
  | `brushDensity` | `float` | `1.0f` |  |
  | `brushFlow` | `float` | `1.0f` |  |
  | `brushErase` | `bool` | `false` |  |
  | `brushTint` | `bool` | `true` | paint the selected brush's coverage in the overlay color |
  | `lastX` | `float` | `0` | Paint: last recorded point, document px |
  | `lastY` | `float` | `0` |  |
  | `curX` | `float` | `0` | Paint: pointer, document px (cursor ring) |
  | `curY` | `float` | `0` |  |
  | `objBg` | `bool` | `false` | Object tool. plain clicks add BACKGROUND points (touchpad-friendly alternative to right-click) |
  | `objPending` | `ObjPending` |  | (nested struct `ObjPending`, see below) |
  | `polyLasso` | `bool` | `false` | Polygon tool. Clicks place / drag / insert vertices; with polyLasso on, a drag draws a freehand outline instead (simplified live while it is drawn). |
  | `polyIdx` | `int` | `-1` | PolyVertex: the vertex being dragged |
  | `lassoRaw` | `std::vector<PolyPt>` |  | Lasso: the pointer's path, document px |
  | `tint` | `std::shared_ptr<CanvasImage>` |  | The red coverage bitmap of the selected brush mask, rebuilt only when the strokes (or the picture's size) change. |
  | `tintShape` | `BrushShape` |  |  |
  | `tintInvert` | `bool` | `false` |  |
  | `lumTint` | `std::shared_ptr<CanvasImage>` |  | The red selection bitmap of the selected luminosity mask. It is built from the picture as it stands just BEFORE that mask (see luminosityTintImage), so it is rebuilt when the shape, the mask's place in the list, or the pixels change. The pixels are keyed by the identity of the images on screen; weak pointers, so no picture is kept alive and no address can be mistaken for a newer image that reuses it. |
  | `lumShape` | `LuminosityShape` |  |  |
  | `lumInvert` | `bool` | `false` |  |
  | `lumSel` | `int` | `-1` |  |
  | `lumKeyImage` | `std::weak_ptr<CanvasImage>` |  |  |
  | `lumKeyPreview` | `std::weak_ptr<CanvasImage>` |  |  |
  | `lumBase` | `std::shared_ptr<CanvasImage>` |  | Small stand-in for state.source the tint is computed on. |
  | `lumBaseSrc` | `std::weak_ptr<CanvasImage>` |  |  |
  | `part` | `int` | `0` | Which part of the selected mask the canvas and the shape sliders edit. Read it through selPart(), which keeps it in range. |
  | `compTint` | `std::shared_ptr<CanvasImage>` |  | The red bitmap of a selected mask with SEVERAL parts: the whole combination, evaluated like the renderer does (see compositeTintImage). |
  | `compParts` | `std::vector<MaskPart>` |  | in-view form, the cache key |
  | `compInvert` | `bool` | `false` |  |
  | `compUsesPix` | `bool` | `false` |  |
  | `compSel` | `int` | `-1` |  |
  | `compW` | `int` | `0` |  |
  | `compH` | `int` | `0` |  |
  | `compKeyImage` | `std::weak_ptr<CanvasImage>` |  |  |
  | `compKeyPreview` | `std::weak_ptr<CanvasImage>` |  |  |
  | `encodedKey` | `uint64_t` | `0` | imageSerial the segmenter has already analyzed |

  - `struct MaskUi::ObjPending`

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `mask` | `int` | `-1` |  |
    | `part` | `int` | `-1` |  |
    | `prompts` | `std::vector<ObjectPrompt>` |  | original-picture fractions |

- `struct FilmEntry`: One picture in the open folder, as the filmstrip shows it.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `path` | `std::string` |  | full path (UTF-8) |
  | `name` | `std::string` |  | file name only |
  | `thumb` | `std::shared_ptr<const CanvasImage>` |  | null until made |
  | `thumbTex` | `mutable std::shared_ptr<CanvasTexture>` |  | The GPU-side copy of `thumb` that paintFilmstrip draws (see filmThumbTexture). Made lazily while painting, hence mutable. |
  | `thumbTexOf` | `mutable std::shared_ptr<const CanvasImage>` |  | what thumbTex shows |
  | `failed` | `bool` | `false` | couldn't be decoded: plain box |
  | `edited` | `bool` | `false` | has saved edits (a sidecar file) |
  | `selected` | `bool` | `false` | in the multi-selection (Sync Settings) |
  | `meta` | `PhotoMeta` |  | rating / flag / color label |
  | `scanned` | `bool` | `false` | `meta` has been read from the sidecar |
  | `copy` | `int` | `0` | 0: the picture's own edits; n: virtual copy n (its `path` is "<file>\|vc<n>", see virtual_copy.hpp; `name` is the file's) |

- `struct FilmStrip`: The pictures of the folder the open picture lives in (JPEG / PNG, natural name order), plus the strip's scroll position. UI thread only.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `files` | `std::vector<FilmEntry>` |  |  |
  | `byName` | `std::unordered_map<std::string, size_t>` |  | lower-case name -> index |
  | `folder` | `std::string` |  |  |
  | `current` | `int` | `-1` | index of the picture that is open |
  | `target` | `int` | `-1` | the one last asked for (== current when idle) |
  | `anchor` | `int` | `-1` | where a Shift-click range starts |
  | `scrollX` | `float` | `0.0f` | pixels scrolled |
  | `viewW` | `float` | `0.0f` | strip width at the last paint |
  | `visible` | `bool` | `true` | View > Toggle Filmstrip |
  | `dirty` | `bool` | `true` | repaint wanted |
  | `thumbVersion` | `uint64_t` | `0` | adjust.version() when the open picture's thumbnail was last (re)made |
  | `filter` | `MetaFilter` |  | the filter bar's state (rating / flag / label) |
  | `view` | `std::vector<int>` |  | indices into `files` the strip shows, ascending (see filmRebuildView) |

- `enum class CompareMode`: What the canvas shows: the edited picture, the same picture with every edit switched off (crop and rotation stay), or both side by side. Values: `Off`, `Before`, `Split`.
- `struct EditorState`

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `original` | `std::shared_ptr<CanvasImage>` |  | untouched decoded pixels; null until something is opened |
  | `oriented` | `std::shared_ptr<CanvasImage>` |  | original rotated/flipped per adjust.orient() (== original if none); crops are measured on this |
  | `orientedFor` | `Orient` |  | the orientation `oriented` has |
  | `orientedPreview` | `std::shared_ptr<CanvasImage>` |  | `oriented` downsampled for live angle drags (null if small) |
  | `rotated` | `std::shared_ptr<CanvasImage>` |  | oriented, straightened by the angle (== oriented at 0 degrees); crops are measured on this |
  | `rotatedAngle` | `float` | `0.0f` | the angle `rotated` was built with |
  | `rotatedExpand` | `bool` | `false` | ...and its corner mode |
  | `source` | `std::shared_ptr<CanvasImage>` |  | rotated with the committed crop applied (== rotated if none) — what adjustments are run on |
  | `sourceCrop` | `CropRect` |  | the crop `source` was built with |
  | `original16` | `std::shared_ptr<Image16>` |  | The 16-bit twin of original / oriented / rotated / source, for a picture that really has more than 8 bits (a 16-bit PNG); all null for any other file. Built from `original16` by the same operations, so each stage has the size of its 8-bit namesake (core/wide_chain.hpp). The 8-bit chain above is unchanged and is still what every tool measures; the full-resolution render (and 16-bit export) starts from the twin, and the neighbor effects read it directly. The drag preview starts from previewBase16 when there is one. |
  | `oriented16` | `std::shared_ptr<Image16>` |  |  |
  | `rotated16` | `std::shared_ptr<Image16>` |  |  |
  | `source16` | `std::shared_ptr<Image16>` |  |  |
  | `image` | `std::shared_ptr<CanvasImage>` |  | source + adjustments applied — this is what's drawn and exported |
  | `imagePath` | `std::string` |  | full path of the opened file |
  | `photoInfo` | `exif::PhotoInfo` |  | its camera data (Info pane); see currentPhotoInfo() |
  | `photoInfoSerial` | `uint64_t` | `~uint64_t{0}` | the imageSerial photoInfo was read for |
  | `colorTag` | `color_tag::ColorTag` |  | what the open file says its color space is (Info pane); see currentColorTag() |
  | `colorTagSerial` | `uint64_t` | `~uint64_t{0}` | the imageSerial colorTag was read for |
  | `colorNoticePending` | `bool` | `false` | the open's color notice may still be wiped by a geometry job; show it again when that lands |
  | `zoom` | `float` | `1.0f` | Zoom. Everything on screen (the canvas size, overlays, hit-testing) is in LOGICAL units, so `zoom` is logical units per image pixel, and every use of it (lx / state.zoom, ctx.scale(state.zoom), ...) stays correct on a HiDPI display. What the user sees ("100%") is device-based: zoomDev is device pixels per image pixel, so 100% really is one image pixel per physical pixel (pixel-exact), and zoom = zoomDev / dpr. Change them only through applyZoomDev() / setZoom() / setDpr(). |
  | `zoomDev` | `float` | `1.0f` |  |
  | `dpr` | `float` | `1.0f` | device pixels per logical unit (LiteUI::contentScale()) |
  | `view` | `View2D` |  | The canvas is as big as the area it fills (not as big as the zoomed picture), so its texture stays window-sized at any zoom. The picture is drawn into it at an offset that we compute: centered when it fits, scrolled by scrollX/scrollY (logical units, in the padded picture) when it doesn't. (nested struct `View2D`, see below) |
  | `adjust` | `AdjustmentSet` |  | current slider values + their undo/redo history |
  | `sidecarSaved` | `uint64_t` | `0` | adjust.version() at the last sidecar save / load |
  | `meta` | `PhotoMeta` |  | the open picture's rating / flag / color label (not an edit: no undo step, see ratePhoto) |
  | `metaDirty` | `bool` | `false` | meta changed since the last sidecar save / load |
  | `snapshots` | `SnapshotList` |  | the open picture's named snapshots: saved in its sidecar, not edits (see addSnapshot) |
  | `snapshotsDirty` | `bool` | `false` | snapshots changed since the last sidecar save / load |
  | `sidecarBlocked` | `bool` | `false` | the open picture's saved-edits file exists but could not be read (see loadSidecar): saveSidecar must not replace it |
  | `exportJpeg` | `bool` | `true` | Export pane settings. the format the Export dialog offers first |
  | `exportPng16` | `bool` | `false` | PNG export writes 16 bits per channel (the pane's "PNG 16-bit"); never set with exportJpeg |
  | `jpegQuality` | `int` | `92` | 1..100 |
  | `keepMetadata` | `bool` | `true` | carry the photo's EXIF + ICC profile into a JPEG |
  | `removeGps` | `bool` | `false` | ...but without the GPS location (JPEG only; needs keepMetadata) |
  | `exportResize` | `bool` | `false` | shrink the export so its long side is exportLongEdge |
  | `exportLongEdge` | `int` | `2048` | px, kExportLongEdgeMin..kExportLongEdgeMax; never enlarges |
  | `exportOverwrite` | `bool` | `false` | batch export: replace files that exist (else "-2", "-3"...) |
  | `film` | `FilmStrip` |  | the open folder's pictures (bottom strip) |
  | `compare` | `CompareMode` | `CompareMode::Off` | Before / After view |
  | `splitPos` | `float` | `0.5f` | Split view: line position, 0..1 of width |
  | `dirty` | `bool` | `true` |  |
  | `adjustDirty` | `bool` | `false` | Set by a slider drag (many events per second while held). The actual pixel recompute is deferred until canvasDirtySource is polled — which happens at most once per rendered frame — instead of running once per drag event, so a flurry of mouse-move events between two frames only costs one recompute, not one per event. |
  | `previewBase` | `std::shared_ptr<CanvasImage>` |  | Interactive-preview path for large images. While a slider is being dragged, adjustments are applied to `previewBase` (`source` box-downsampled to roughly kPreviewMaxEdge on its long side) instead of the full-resolution original, and the result (`previewImage`) is drawn stretched to the document's full size. One full-resolution recompute runs when the drag ends (pointer up / cancel). `previewBase` is null for images already small enough that a preview wouldn't be cheaper, in which case dragging just uses the full-resolution path. |
  | `previewBase16` | `std::shared_ptr<Image16>` |  | The 16-bit twin of `previewBase` (box-downsampled from `source16` by the same factor, so the same size), for a picture that has a 16-bit twin; null otherwise. A drag renders it, so the preview starts from the same precision as the full-resolution render that follows it. `previewBase` is then only its 8-bit view. |
  | `previewImage` | `std::shared_ptr<CanvasImage>` |  |  |
  | `dragging` | `bool` | `false` |  |
  | `crop` | `CropUi` |  | live crop selection; only shown/editable while the Crop pane (activity 2) is open |
  | `maskSel` | `int` | `-1` | Masks pane (activity 7). The masks themselves live in `adjust` (so they are undoable); this is only which one is selected and how it is shown. index into adjust.masks(); read it through selMask() |
  | `maskShowTint` | `bool` | `false` | paint the selected mask's weight in the overlay color |
  | `maskTintColor` | `int` | `0` | index into kMaskTintPalette (session only, not saved) |
  | `mask` | `MaskUi` |  |  |
  | `activeActivity` | `int` | `0` | -1 = sidebar closed |
  | `sidebarWidth` | `float` | `220.0f` |  |
  | `maskPanelW` | `float` | `300.0f` | right-hand mask panel (Masks pane only) |
  | `maskTab` | `int` | `0` | its tab: 0 Effect, 1 Shape, 2 Parts |
  | `maskFx` | `int` | `0` | Effect tab: 0 Basic, 1 Mix, 2 Grade, 3 Curve |
  | `openMenu` | `int` | `-1` | -1 = no dropdown open, else its menu id |
  | `maximized` | `bool` | `false` | mirrors our own toggle (no getter in LiteUI) |
  | `statusMsg` | `std::string` |  | One-line message shown in the status bar after the image info: open and export failures, and export success. Cleared by the next open, export or edit (see clearStatus's callers). |
  | `statusIsError` | `bool` | `false` |  |
  | `previewDocW` | `float` | `0.0f` | While an angle drag shows its low-res preview, the picture's size is changing every frame but `image` (full-res) isn't rebuilt until the drag ends — so the document size comes from these overrides instead. |
  | `previewDocH` | `float` | `0.0f` |  |
  | `dragIsAngle` | `bool` | `false` | the drag in progress is the angle slider |
  | `imageSerial` | `uint64_t` | `0` | bumps on every opened file (keys segmenter caches) |
  | `histR` | `std::array<uint32_t, 256>` | `{}` | Histogram of what is currently shown (the mid-drag preview while a slider is held, else the full-resolution result). Recomputed lazily by the histogram canvas's dirty poll when `histStale` is set; `histVersion` bumps on every recompute so that canvas knows to repaint. |
  | `histG` | `std::array<uint32_t, 256>` | `{}` |  |
  | `histB` | `std::array<uint32_t, 256>` | `{}` |  |
  | `histL` | `std::array<uint32_t, 256>` | `{}` |  |
  | `histStale` | `bool` | `true` |  |
  | `histVersion` | `uint32_t` | `0` |  |
  | `scopeKind` | `scopes::Kind` | `scopes::Kind::Histogram` | What the pinned canvas above the panes shows; a click on it cycles Histogram -> Waveform -> Vectorscope (cycleScope). scopeImage is the waveform or vectorscope picture, rebuilt by computeHistogram() together with the histogram; it is null while the histogram itself is showing (and for an empty picture), so the histogram mode costs nothing extra. |
  | `scopeImage` | `std::shared_ptr<CanvasImage>` |  |  |
  | `clipShadowsOn` | `bool` | `false` | Clipping warnings. The flags say which overlays are on; histClipLo/Hi are the channels (bit 0/1/2 = R/G/B) that have any clipped pixel in the shown picture (drives the triangle colors). clipPerChannel colors the overlay by clipped channel (Shift+J) instead of classic red/blue. clipOverlay is rebuilt by computeHistogram and is null while both overlays are off. |
  | `clipHighlightsOn` | `bool` | `false` |  |
  | `histClipLo` | `uint8_t` | `0` |  |
  | `histClipHi` | `uint8_t` | `0` |  |
  | `clipPerChannel` | `bool` | `false` |  |
  | `clipOverlay` | `std::shared_ptr<CanvasImage>` |  |  |
  | `hoverValid` | `bool` | `false` | RGB readout under the cursor (status bar). hoverValid is false while the pointer isn't over a picture pixel. |
  | `hoverR` | `uint8_t` | `0` |  |
  | `hoverG` | `uint8_t` | `0` |  |
  | `hoverB` | `uint8_t` | `0` |  |
  | `wbPicking` | `bool` | `false` | White-balance picker (a button in the Adjust pane, key W). While armed, one click on the picture sets Temperature and Tint so that spot turns gray. Read it through wbPickerActive(), which also drops it when it no longer applies. |

  - `bool showPreview() const`: True while `previewImage` stands in for `image`: during a drag, and after it until the background full-resolution render lands (renderWorker).
  - `bool previewDocActive() const`: True while an angle drag is showing its low-res preview: `dragging`, a `previewImage` exists and `previewDocW > 0`.
  - `float docWidth() const`: Document width in image pixels: the preview override while `previewDocActive()`, else `image->width`, else `kPlaceholderW` before anything is open.
  - `float docHeight() const`: Document height in image pixels, with the same three-way rule as `docWidth()` (`kPlaceholderH` as the fallback).
  - `void markDirty()`: Asks for a canvas repaint at the next frame.
  - `void markAdjustDirty()`: Asks for the deferred pixel recompute; it runs when `canvasDirtySource` is next polled (see `adjustDirty`).
  - `struct EditorState::View2D`: The canvas is as big as the area it fills (not as big as the zoomed picture), so its texture stays window-sized at any zoom. The picture is drawn into it at an offset that we compute: centered when it fits, scrolled by scrollX/scrollY (logical units, in the padded picture) when it doesn't.

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `w` | `float` | `0` | canvas size, from onLayout |
    | `h` | `float` | `0` |  |
    | `scrollX` | `float` | `0` |  |
    | `scrollY` | `float` | `0` |  |
    | `drag` | `enum class Drag` | `Drag::None` | Values: `None`, `Pan`, `VThumb`, `HThumb`. |
    | `dragX` | `float` | `0` | pointer at press |
    | `dragY` | `float` | `0` |  |
    | `dragScrollX` | `float` | `0` | scroll at press |
    | `dragScrollY` | `float` | `0` |  |


**State and tables**

- `kCropAspects`: `static const std::array<CropAspect, 6>`, initially `{{ {"Free", 0.0f}, {"Original", -1.0f}, {"1:1", 1.0f}, {"4:3", 4.0f /…`
- `state`: `static EditorState`

**Functions**

**`static void setStatus(std::string msg, bool isError)`**: Shows `msg` in the status bar. Kept to one short line: newlines (library error strings sometimes have them) become spaces, and long text is cut on a UTF-8 boundary with an ellipsis.

**`static void clearStatus()`**: Empties the status-bar message and its error flag. Called by every action that makes an earlier message stale (the next open, export or edit).

**`static void applyZoomDev(float zoomDev)`**: Sets the zoom from a device-pixel zoom (1.0 = 100%, pixel-exact) and derives the logical scale everything else uses. Does not mark dirty.

### `ui_state.hpp`
UI-only state, the part of the old pile of `g...` globals that is just "how the UI is currently showing things": which tab is selected, which list row was clicked, the Copy Settings clipboard, the name fields' text states and the curve editors' selected points. Kept separate from `EditorState` on purpose: nothing in it is an edit or part of the picture. One instance, `uiState`. UI thread only. It also owns the three **reset helpers** that keep a stored index from pointing at something that no longer exists (a new picture, undo, a different mask).

**Depends on:** engine: `color_grade.hpp`, `color_mix.hpp`, `curves.hpp`, `preset.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `struct PresetEntry`: One row of the Presets list: a preset file's display name, its path and what it holds.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `name` | `std::string` |  |  |
  | `path` | `std::string` |  |  |
  | `preset` | `Preset` |  |  |

- `struct MaskEffects`: What Copy / Paste Effects moves between masks: everything but the shape.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `amount` | `float` | `100.0f` |  |
  | `adj` | `AdjValues` | `{}` |  |
  | `mix` | `MixValues` | `{}` |  |
  | `grade` | `ColorGrade` |  |  |
  | `curves` | `CurveSet` | `identityCurves()` |  |

- `struct UiState`: The one UI-state object, grouped by pane (see the fields below). The instance is `uiState`.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `mix` | `Mix` |  | Color Mixer pane (nested struct `Mix`, see below) |
  | `grade` | `Grade` |  | Color Grading pane (nested struct `Grade`, see below) |
  | `curve` | `Curve` |  | Tone curve editor (canvas side) (nested struct `Curve`, see below) |
  | `maskPane` | `MaskPane` |  | Masks pane, Effect tab (Mix / Grade / Curve) (nested struct `MaskPane`, see below) |
  | `presets` | `Presets` |  | Presets pane: the files in the presets folder (a cache, re-read when the pane is opened and after a save or delete) and the last row clicked. (nested struct `Presets`, see below) |
  | `snapshots` | `Snapshots` |  | Snapshots pane (nested struct `Snapshots`, see below) |
  | `clipboard` | `Clipboard` |  | In-memory clipboards (nested struct `Clipboard`, see below) |
  | `names` | `Names` |  | The TextInput of a name field keeps its text in a shared state; we hold a pointer to it so the field can follow the selection (selection change, undo / redo) and a command can read and clear it. (nested struct `Names`, see below) |
  | `hist` | `Hist` |  | Width of the histogram canvas as last painted (for corner hit-testing). (nested struct `Hist`, see below) |
  | `vcDeleteArmed` | `std::string` |  | Path of the copy "Delete Virtual Copy" was pressed for (press it twice). |

  - `void dropCurveSelection()`: A curve editor's selected point is an index into a curve, and a snapshot row is an index into the picture's snapshot list. Both stop meaning anything when the edits they point into are replaced, so the code that replaces them calls these (new picture, undo / redo / History jump, a preset or snapshot, a different mask). The Curve pane's selected point. repaint without a picture change
  - `void dropMaskCurveSelection()`: The Masks pane's Curve tab: its point belongs to the selected mask.
  - `void dropCurveSelections()`
  - `void resetForNewPicture()`: A different picture (or virtual copy) was opened: nothing selected for the previous one applies. The Presets selection stays: presets are files, not part of any picture.
  - `struct UiState::Mix`: Color Mixer pane

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `chan` | `MixChan` | `MixChan::Hue` | which of Hue / Sat / Lum is shown |

  - `struct UiState::Grade`: Color Grading pane

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `mode` | `int` | `0` | 0 = 3-way, 1..4 = Shadows..Global |
    | `active` | `int` | `1` | wheel shown in the H/S/L readout (Midtones) |

  - `struct UiState::Curve`: Tone curve editor (canvas side)

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `active` | `int` | `-1` | point being dragged, or -1 |
    | `channel` | `CurveChannel` | `CurveChannel::Master` | curve being edited |
    | `uiDirty` | `bool` | `false` | Set by UI-only changes (channel switch, selecting a point) that don't change the picture, so the curve canvas still repaints; consumed by its dirty poll. |
    | `grabDx` | `float` | `0.0f` | (point - mouse) in curve units at press, so a grabbed point doesn't jump to the cursor's exact position. |
    | `grabDy` | `float` | `0.0f` |  |

  - `struct UiState::MaskPane`: Masks pane, Effect tab (Mix / Grade / Curve)

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `mixChan` | `MixChan` | `MixChan::Hue` | Mix: Hue / Sat / Lum |
    | `gradeRange` | `int` | `1` | Grade: 0..3 (Midtones) |
    | `curveCh` | `CurveChannel` | `CurveChannel::Master` | Curve: channel |
    | `curveActive` | `int` | `-1` | point being dragged |
    | `curveUiDirty` | `bool` | `false` | repaint-only change |
    | `curveGrabDx` | `float` | `0.0f` |  |
    | `curveGrabDy` | `float` | `0.0f` |  |

  - `struct UiState::Presets`: Presets pane: the files in the presets folder (a cache, re-read when the pane is opened and after a save or delete) and the last row clicked.

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `list` | `std::vector<PresetEntry>` |  |  |
    | `sel` | `int` | `-1` | the last preset clicked, or -1 |

  - `struct UiState::Snapshots`: Snapshots pane

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `sel` | `int` | `-1` | the last snapshot clicked, or -1 |

  - `struct UiState::Clipboard`: In-memory clipboards

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `settings` | `std::optional<Preset>` |  | Copy / Paste Settings: same payload as a preset (the look only), kept in memory instead of a file, and where it was copied from. |
    | `settingsFrom` | `std::string` |  |  |
    | `maskEffects` | `std::optional<MaskEffects>` |  | Copy / Paste Effects |

  - `struct UiState::Names`: The TextInput of a name field keeps its text in a shared state; we hold a pointer to it so the field can follow the selection (selection change, undo / redo) and a command can read and clear it.

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `mask` | `std::shared_ptr<TextInputState>` |  | the Masks pane's name field |
    | `preset` | `std::shared_ptr<TextInputState>` |  | the Presets pane's name field |
    | `snapshot` | `std::shared_ptr<TextInputState>` |  | the Snapshots pane's name field |

  - `struct UiState::Hist`: Width of the histogram canvas as last painted (for corner hit-testing).

    | Field | Type | Default | Meaning |
    |---|---|---|---|
    | `canvasW` | `float` | `0.0f` |  |


**State and tables**

- `uiState`: `static UiState`

### `canvas_view.hpp`
The canvas view: where the picture sits inside the canvas and how the user moves it. The picture is `state.docWidth() × docHeight()` image pixels shown at `state.zoom` logical units per pixel, with `kViewPad` of margin all round. On an axis where it fits it is centered; otherwise that axis scrolls. Zoom, scrolling, the two scrollbars and the canvas-to-picture coordinate conversion are all here. All positions are in logical units; the **device**-pixel zoom (`zoomDev`, 1.0 = 100%) is what the user sees, and `zoom = zoomDev / dpr`.

**Depends on:** `render_update.hpp` (defined later), `crop.hpp` (defined later), `geometry_worker.hpp` (defined later), `saved_edits.hpp` (defined later). **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kViewPad` | `float` | `24.0f` | Margin around the picture inside the scrollable content, on every side |
| `kViewBar` | `float` | `kScrollbarThickness` | Scrollbar thickness (`kScrollbarThickness` from LiteUI) |
| `kViewMinThumb` | `float` | `24.0f` | Shortest a scrollbar thumb may get, so it stays grabbable on a huge picture |

**Types**

- `using ViewDrag = decltype(EditorState::view)::Drag`: The `Drag` enum of `EditorState::view` (`None`, `Pan`, `VThumb`, `HThumb`), under a shorter name.
- `struct ViewGeo`: The view's geometry for one frame (see `viewGeo()`); all values are logical units.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `W` | `float` | `0` | picture size on screen |
  | `H` | `float` | `0` |  |
  | `cw` | `float` | `0` | padded content size |
  | `ch` | `float` | `0` |  |
  | `hbar` | `bool` | `false` |  |
  | `vbar` | `bool` | `false` |  |
  | `baseX` | `float` | `0` | picture origin in the canvas at scroll 0 |
  | `baseY` | `float` | `0` |  |
  | `maxX` | `float` | `0` | largest scroll offset |
  | `maxY` | `float` | `0` |  |

- `struct ViewThumb`: A scrollbar thumb along a track: start offset and length.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `pos` | `float` |  |  |
  | `len` | `float` |  |  |


**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static void syncSource(bool force)`
- `static void updatePreviewFromAdjustments()`
- `static bool geometryDirty()`: Defined after GeometryWorker (needs its type); forward-declared here because updateImageFromAdjustments (right below) is written once, in its natural place, even though it now has to decide between the synchronous pipeline and GeometryWorker before either exists yet in the file.
- `static void submitGeometryJob()`
- `static void announceColorTag()`

**Functions**

**`static ViewGeo viewGeo(bool clampScroll = true)`**: **Input:** `clampScroll` (default true). **Returns:** the `ViewGeo` for the current zoom and canvas size: `W`/`H` = picture size on screen, `cw`/`ch` = that plus `kViewPad` on every side, a bar is needed on an axis when the content is more than 0.5 larger than the canvas, `maxX`/`maxY` = the largest scroll offsets, and `baseX`/`baseY` = the picture origin at scroll 0 (centered when there is no bar on that axis, else `kViewPad`). With `clampScroll` it also pulls `state.view.scrollX/Y` back into `[0, max]`, because the picture can change size under the view (crop, rotate, new zoom). Pass `false` to read the geometry without moving the scroll.

**`static void viewOrigin(const ViewGeo &g, float &ox, float &oy)`**: **Inputs:** a `ViewGeo` and two out-references. **Result:** the picture's top-left corner in canvas coordinates, `base - scroll`, rounded to a whole **device** pixel so a 100% picture is drawn pixel-exact at any display scale.

**`static void viewLocal(float x, float y, float &lx, float &ly)`**: **Inputs:** canvas coordinates `(x, y)`. **Outputs:** `lx = x - originX`, `ly = y - originY`, the position relative to the picture's corner. This is what the crop, mask, picker and readout code take; divide by `state.zoom` to get image pixels.

**`static void changeView(float newDpr, float newZoomDev, float ax, float ay)`**: **Inputs:** the new display scale `newDpr` (clamped to at least 1), the new device zoom `newZoomDev` (clamped to `kZoomMin..kZoomMax` by `applyZoomDev`), and an anchor `(ax, ay)` in canvas coordinates. **Effect:** keeps the image point that was under the anchor under it after the change, re-clamps the scroll and marks the canvas dirty. With no picture it just applies the zoom.

**`static void zoomAbout(float zoomDev, float ax, float ay)`**: Zooms to `zoomDev` about the canvas point `(ax, ay)` (used by the mouse wheel and pinch). Same as `changeView` with the current display scale.

**`static void setZoom(float zoomDev)`**: Zooms about the middle of the canvas (menus, buttons, shortcuts).

**`static void setDpr(float dpr)`**: The display scale changed (first known at startup, or the window moved to another monitor): keep the same device zoom, so 100% stays 100%.

**`static ViewThumb viewThumb(float track, float content, float scroll, float maxScroll)`**: **Inputs:** the track length, the content length, the scroll offset and the largest scroll offset along one axis. **Returns:** `{pos, len}` of the scrollbar thumb: `len = clamp(track² / content, kViewMinThumb, track)` and `pos` proportional to `scroll / maxScroll` over the free part of the track (0 when nothing scrolls).

**`static bool viewBarPress(float x, float y)`**: A press at canvas `(x, y)`. If it lands on the vertical or horizontal scrollbar: on the thumb it starts a `VThumb` / `HThumb` drag (recording the pointer and the scroll at press); on the track it pages the view by one canvas height / width toward the click. **Returns** true if the press was on a bar (so the canvas must not treat it as a picture click). False when no picture is open.

**`static void viewPanBegin(float x, float y)`**: Starts a `Pan` drag at canvas `(x, y)`, recording the press point and the scroll at that moment. `viewDragTo` then moves the scroll opposite to the pointer.

**`static void viewDragTo(float x, float y)`**: Continues the drag begun by `viewBarPress` / `viewPanBegin` to canvas `(x, y)`: `Pan` scrolls by the pointer's movement; a thumb drag scrolls in proportion to the thumb's free range. Re-clamps the scroll and marks the canvas dirty. Does nothing without a picture or without a drag.

**`static void paintViewBars(CanvasContext &ctx, const ViewGeo &g)`**: **Inputs:** the canvas context and the `ViewGeo` for this frame. Draws a track and a rounded thumb for each visible scrollbar, and the small corner square where both are visible.

### `render_update.hpp`
Keeps `state.image` (what is drawn and exported) up to date with `state.adjust`. Two paths: a **deferred** one for large pictures (show a cheap preview at once, let `renderWorker` produce the full-resolution result in the background) and a **synchronous** one for everything else. The worker announces a finished result by waking the event loop (`RenderWorker`'s notify callback is wired to `LiteUI::wake` in `main()`), the loop polls the canvases, and `collectFullRender()` picks the result up from there. While no render is outstanding nothing runs, so an idle editor costs nothing.

**Depends on:** `render_worker.hpp`, `crop.hpp` (defined later), `geometry_worker.hpp` (defined later); engine: `adjustment_set.hpp`, `latest_worker.hpp`, `raster_ops.hpp`, `render.hpp`, `render_core.hpp`, `thumb_worker.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void adoptFullRender(RenderWorker::Result &&r)`**: **Input:** the worker's `RenderWorker::Result`. Makes its image `state.image`, drops the stand-in `previewImage`, marks the histogram stale and the canvas dirty.

**`static void collectFullRender()`**: Non-blocking: asks `renderWorker.collect()` for a finished, still-current result and adopts it. Polled once per frame from `flushPendingAdjust`. Nothing here catches: `runRenderJob` does not catch either, so if a render job throws, `collect()` rethrows it on the UI thread from this function.

**`static void finishFullRender()`**: Blocking: if a render is outstanding, waits for it (`waitForLatest`) and acts on its `WaitStatus`, so `state.image` is current when it returns. For the few callers that need the real pixels *now* (export). Returns at once when nothing is outstanding.
- `completed`: adopts the image (`adoptFullRender`); a `completed` status with no value throws `std::logic_error`.
- `cancelled` (superseded, invalidated or abandoned): adopts nothing and **throws** `std::runtime_error("Render was cancelled before it completed")`, so the caller must not assume `state.image` was refreshed.
- `failed`: rethrows the job's exception (or throws `std::runtime_error` if there is none).

**`static void enterDrag(bool isAngle)`**: **Input:** whether the drag is the angle slider. A drag starts (or continues) on the preview path. If it is a new drag, any full-resolution render in flight is invalidated, because it is for values the drag is about to change. Sets `state.dragging` and `state.dragIsAngle`, which switches recompute and paint to the preview path.

**`static void updateImageFromAdjustments()`**
Rebuilds `state.image` from the untouched `state.source` pixels with the current adjustment values. Called after every change to `state.adjust`: a slider drag, undo, redo, a crop, opening a picture.
- Does nothing until a picture is open. Clears the status message and `colorNoticePending`, and invalidates any render in flight.
- **Geometry on the worker:** on a large picture (`isLargeImage`) with a pending orient / straighten / crop change (`geometryDirty()`), it calls `submitGeometryJob()` and returns; the current picture stays on screen until the geometry bundle lands (see `geometry_worker.hpp`).
- Otherwise `syncSource(false)` picks up a crop change from apply / undo / redo. If `source` did not change, a preview base exists and `image` still has the source's size, it takes the **deferred path**: render the preview now (`updatePreviewFromAdjustments`) so the picture updates immediately, and `renderWorker.submit(RenderJob{source, buildRenderParams(), source16})` for the full-resolution result. `paintCanvas` keeps showing the preview (`EditorState::showPreview`) until it arrives.
- Otherwise the **synchronous path**: render into `state.image` now (from the 16-bit twin with `renderToDisplay`, else `renderAdjusted`), drop the preview and the preview document size, mark the histogram stale and the canvas dirty. Used for small pictures, where the pass is faster than the hand-off, and for any change that alters the picture's geometry, because then `source`, `image`, the document size and the crop selection must all change together.

**`static void updatePreviewFromAdjustments()`**: Like `updateImageFromAdjustments()` but applied to the small preview base (from its 16-bit twin if there is one). Only touches `state.previewImage` (and marks the histogram stale and the canvas dirty); `state.image`, the document size, export and the info labels are left alone until the drag ends. No-op when there is no preview base (a small picture just uses the full path).

## Crop, geometry and rotation

### `crop.hpp`
The crop selection helpers, all on `state.crop` (the live selection) and `state.source`: whether the tool is active, the aspect lock, fitting and resetting the selection, and `syncSource()`, which rebuilds the picture stages (`oriented` → `rotated` → `source`) whenever the committed orientation, angle or crop no longer matches what they were built with. The pointer handling is in `crop_tool.hpp`; the painting in `crop_overlay.hpp`.

**Depends on:** engine: `geometry.hpp`, `raster_ops.hpp`, `wide_chain.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static bool cropToolActive()`**: True while the Crop pane (activity 2) is open and a picture is loaded. The selection is only shown and edited then.

**`static float cropAspect()`**: **Returns** the width / height ratio the selection is locked to, or 0 for free-form. A positive table ratio is returned as is; the "Original" entry (negative ratio) returns the aspect of the picture being cropped (`source` width / height, or 0 if there is none).

**`static void cropFitToAspect()`**: Does nothing for free-form or an empty selection. Otherwise shrinks the selection about its center so `w / h` equals the lock (the longer dimension is cut), so it always stays inside where it already was.

**`static void resetCropSelection()`**: Sets the selection to the whole of `state.source` (0, 0, width, height), cancels any drag, then fits it to the aspect lock.

**`static void setCropAspect(size_t idx)`**: **Input:** an index into `kCropAspects`. Stores it in `state.crop.aspectIdx`, re-fits the selection to the new lock and repaints.

**`static void syncSource(bool force)`**
**Input:** `force`, true when a new picture is opened. Rebuilds the stages that no longer match `state.adjust`, in order:
- **oriented** (rotate / flip) when the orientation changed or `force`: also its 16-bit twin and `orientedPreview` (a downsampled copy for live angle drags).
- **rotated** (straighten) when the orientation or the angle changed, or the corner mode changed while the angle is not 0 (the corner mode only matters when there is an angle). Equal to `oriented` at angle 0.
- **source**: `rotated` with the committed crop applied (or `rotated` itself if there is no crop), its 16-bit twin, and a new `previewBase` / `previewBase16`; the preview image is dropped.
- Finally `resetCropSelection()`, because the image the selection is measured against just changed size.
Returns early without changing anything when nothing is out of date (and `force` is false).

### `geometry_worker.hpp`
The background **geometry worker**: it runs the whole orient → straighten → crop → render pipeline off the UI thread for a large photo, so tapping Rotate or Straighten's nudge buttons on a 24-megapixel picture doesn't freeze the window. Same thread model and cancellation contract as `RenderWorker` (one lazily started thread, a generation number supersedes stale work, the running job polls it and abandons within a few scanlines). The difference: a render job can start from `state.source`, but a geometry job may be *changing* the source, so it always rebuilds the whole chain from `original`. That wastes a little work when only the crop changed after a straighten, but keeps a job a pure function of its inputs. Holds the UI-thread glue too: the dirty test, submit, adopt, collect and finish functions, and `geometryBusy()`. The `// BEGIN geometryworker` / `// END geometryworker` fence covers the worker proper.

**Depends on:** `crop.hpp`, `saved_edits.hpp` (defined later); engine: `adjustment_set.hpp`, `geometry.hpp`, `image16.hpp`, `latest_worker.hpp`, `raster_ops.hpp`, `render_core.hpp`, `render_params.hpp`, `thumb_worker.hpp`, `wide_chain.hpp`. **Test:** the worker protocol is the engine's `LatestWorker`, tested in `tests/cases/latest_worker.inc`; this file's own code has no case of its own. It is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `using GeometryWorker = LatestWorker<GeometryJob, GeometryResult>`: `LatestWorker<GeometryJob, GeometryResult>`.
- `struct GeometryBundle`: Exactly the geometry-derived `EditorState` fields (`oriented`, `rotated`, `source`, `previewBase`, the 16-bit twins, `image`, ...), adopted as one atomic swap on the UI thread (see `adoptGeometryBundle`) so the picture, its document size and the crop selection always change together.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `oriented` | `std::shared_ptr<CanvasImage>` |  |  |
  | `orientedFor` | `Orient` |  |  |
  | `orientedPreview` | `std::shared_ptr<CanvasImage>` |  |  |
  | `rotated` | `std::shared_ptr<CanvasImage>` |  |  |
  | `rotatedAngle` | `float` | `0.0f` |  |
  | `rotatedExpand` | `bool` | `false` |  |
  | `source` | `std::shared_ptr<CanvasImage>` |  |  |
  | `sourceCrop` | `CropRect` |  |  |
  | `previewBase` | `std::shared_ptr<CanvasImage>` |  |  |
  | `previewBase16` | `std::shared_ptr<Image16>` |  | the 16-bit twin of previewBase (null for an 8-bit file or a small picture) |
  | `image` | `std::shared_ptr<CanvasImage>` |  |  |
  | `oriented16` | `std::shared_ptr<Image16>` |  | the 16-bit twin (null for an 8-bit file) |
  | `rotated16` | `std::shared_ptr<Image16>` |  |  |
  | `source16` | `std::shared_ptr<Image16>` |  |  |

- `struct GeometryResult`: What the worker returns: one `GeometryBundle`.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `bundle` | `GeometryBundle` |  |  |

- `struct GeometryJob`: One request: a snapshot of everything the pipeline needs (`original`, orientation, angle, corner mode, crop, `RenderParams`, and the 16-bit original if any). `original` is not `const` only because the pipeline's functions take the same `shared_ptr<CanvasImage>` type `state.original` is; it must never be modified (images are replaced, never edited in place).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `original` | `std::shared_ptr<CanvasImage>` |  | Non-const, unlike RenderJob::src: orientImage (identity case) and the rest of this pipeline are written in terms of the same shared_ptr<CanvasImage> type `state.original` already is, so this just carries that type through - never a license to mutate it. |
  | `orient` | `Orient` |  |  |
  | `angle` | `float` | `0.0f` |  |
  | `expand` | `bool` | `false` |  |
  | `crop` | `CropRect` |  |  |
  | `renderParams` | `RenderParams` |  |  |
  | `original16` | `std::shared_ptr<Image16>` |  | the 16-bit twin of `original`, if the picture has one |


**State and tables**

- `geometryWorker`: `static GeometryWorker`, initially `{runGeometryJob}`

**Functions**

**`static std::optional<GeometryResult> runGeometryJob(GeometryJob &&job, WorkerCancel c)`**: **Worker thread only.** The whole chain from `job.original`, checking `c.cancelled()` between every stage and by row / block inside the two per-pixel passes (`rotateArbitrary`, `renderWith`), so a superseded job abandons within milliseconds. In order: `orientImage` (identity: no copy), the orientation preview base, `rotateArbitrary` at the job's angle (skipped at 0), the crop, the 16-bit twin chain (`wide_chain::oriented` / `rotated` / `cropped`) when there is one, `makePreviewBase` for the 8-bit and 16-bit sources, and finally the render of `image` (from the 16-bit twin with `renderToDisplay` if present, else `renderWith`). **Returns** `std::nullopt` if abandoned at any point, else the finished `GeometryBundle`.

**`static bool geometryDirty()`**: True when the committed orientation / angle / corner mode / crop in `state.adjust` hasn't been rendered into `state.oriented` / `rotated` / `source` yet. Mirrors `syncSource`'s own dirty checks as a read-only predicate (no rebuilding). Also true while a geometry job is outstanding or while `oriented` / `rotated` are missing; checking both directly means a bug elsewhere fails safe (one wasted resubmit) instead of adopting a stale in-flight job.

**`static void submitGeometryJob()`**: Snapshots the current `original`, orientation, angle, corner mode, crop, `RenderParams` and 16-bit original into a `GeometryJob`, submits it (superseding any job in flight) and shows "Applying changes..." in the status bar.

**`static void adoptGeometryBundle(GeometryWorker::Result &&r)`**: **Input:** a finished result. Swaps in every field of the bundle together (`oriented`, `rotated`, `source`, `previewBase`, the 16-bit twins, `image`, ...), so the picture, its document size and the crop selection never show a combination that belonged to two different requests. Then drops the preview image and preview document size, resets the crop selection to the new source, marks the histogram stale, clears the "Applying changes..." message, re-announces a freshly opened picture's color notice if it was pending (`colorNoticePending`) and repaints.

**`static void collectGeometryJob()`**: Non-blocking; polled once per frame from `flushPendingAdjust`, alongside `collectFullRender`. Adopts a finished bundle (`adoptGeometryBundle`). If the job threw, `collect()` rethrows and this catches a `std::exception`, showing "Rebuild failed: <reason>" as an error in the status bar (a non-`std::exception` throw is not caught here).

**`static void finishGeometryJob()`**: Blocking: makes every field a geometry rebuild touches current. For the one place that needs the real pixels *now* (export). Returns at once when nothing is outstanding; otherwise it `waitForLatest()`s and acts on the `WaitStatus`: `completed` adopts the bundle (a missing value throws `std::logic_error`); `cancelled` adopts nothing, so no geometry field is updated half-way, and **throws** `std::runtime_error("Geometry rebuild was cancelled before completion")`; `failed` rethrows the job's exception (or throws `std::runtime_error` if there is none).

**`static bool geometryBusy()`**: True while a crop / rotate / flip / straighten rebuild is still running (`geometryWorker.outstanding()`). Until it lands, `state.rotated` / `state.source` / the crop selection describe the *previous* geometry while `state.adjust` already holds the new one, so any edit that measures against them (a second rotate remapping the crop rect, Apply Crop reading the selection) would use stale sizes. Those edits are ignored while this is true, and their buttons are dimmed to match.

**`static void collectGeometryJob()`**: Non-blocking: adopts a finished, still-current geometry result if there is one. Polled once per frame from `flushPendingAdjust`, alongside `collectFullRender`.

**`static void finishGeometryJob()`**: Blocking: waits for an outstanding geometry job and adopts it, so every field a geometry rebuild touches is current. For the one place that needs the real pixels *now* (export).

**`static bool geometryBusy()`**: True while a crop / rotate / flip / straighten rebuild is still running. Until it lands, `state.rotated`, `state.source` and the crop selection describe the previous geometry while `state.adjust` already holds the new one, so any edit that measures against them would compute from stale sizes. Such edits (`applyGeoOp`, `resetOrientation`, `applyCrop`, `removeCrop`) return at once while this is true, and their buttons are dimmed.

### `crop_tool.hpp`
The Crop tool's pointer logic and its two commands. Hit-testing which handle is under the pointer, dragging out / moving / resizing the selection (honoring the aspect lock and the image bounds), and Apply / Remove Crop, each one undo step. Coordinates passed to the handlers are canvas-local screen pixels; dividing by `state.zoom` gives image pixels.

**Depends on:** `render_update.hpp`, `crop.hpp`, `geometry_worker.hpp`; engine: `adjustment_set.hpp`, `geometry.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static int cropHitTest(float px, float py)`**: **Inputs:** a point in image coordinates. **Returns:** `kEdge*` bits when the point is within `kCropHitPx / zoom` of a handle (one bit = an edge, two bits = a corner), `kEdgeMove` for the interior, or 0 for none. Edge tests need the point within the selection's span on the other axis (plus the tolerance).

**`static void cropDragCorner(float ax, float ay, float px, float py)`**: Sets the selection to the rectangle spanned by a fixed corner `(ax, ay)` and the pointer `(px, py)`, honoring the aspect lock and the image bounds: with a lock, it grows to whichever dimension asks for more and is then clamped to the room left on that side. Used for both dragging out a new selection and dragging a corner handle.

**`static void cropResize(int mask, float px, float py)`**: **Inputs:** a `kEdge*` mask and the pointer. A corner mask drags the corner (via `cropDragCorner`, anchored at the opposite corner). A single edge moves that edge; with an aspect lock the perpendicular dimension follows about the selection's center, limited by the room on both sides. The size never drops below `min(kCropMinPx, W, H)`.

**`static void cropPress(float lx, float ly)`**: Pointer press at canvas-local `(lx, ly)`. Closes any open menu. When the tool is active: on the interior starts a `Move` (remembering the grab offset), on a handle starts a `Resize` with that mask, and on empty area starts a `New` selection anchored there. The old selection stays until the pointer actually moves, so a plain click doesn't wipe it out.

**`static void cropDrag(float lx, float ly)`**: Pointer drag at `(lx, ly)`: clamps to the picture and continues the active drag: `New` → `cropDragCorner`, `Resize` → `cropResize`, `Move` → shifts the rectangle, clamped inside the picture. Repaints. No-op unless the tool is active and a drag is in progress.

**`static void applyCrop()`**: Commits the selection as the crop, one undo step. Ignored while a geometry job is busy, with no picture, or when the rounded selection is smaller than 1×1 or covers everything. The selection is relative to the already cropped `source`, but the stored crop is relative to the untouched original, so the existing crop offset is added before `adjust.setCrop`. Then `updateImageFromAdjustments()` (whose `syncSource` rebuilds the source and the selection).

**`static void removeCrop()`**: Drops the committed crop, restoring the full original (one undo step). Ignored while a geometry job is busy or when there is no crop.

### `rotate.hpp`
Rotate / flip (the 90-degree steps) and the Straighten angle, each as an undo step, plus `undo()` and `redo()` for the whole editor. Rotate and flip go through the engine's `geoStep`, which remaps the crop rectangle so it stays on the same part of the picture. Straighten records the value and defers the resample to the canvas's per-frame dirty poll, so a drag costs one resample per frame.

**Depends on:** `render_update.hpp`, `geometry_worker.hpp`; engine: `adjustment_set.hpp`, `geometry.hpp`, `raster_ops.hpp`, `render.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void applyGeoOp(GeoOp op)`**: **Input:** a `GeoOp` (rotate left / right, flip horizontal / vertical). One undo step. Applies `geoStep` to a copy of the orientation, angle and crop using the size of `state.rotated` (what crops are measured against), stores the result with `adjust.setGeometry` and calls `updateImageFromAdjustments()`. Ignored without a picture or while a geometry job is busy.

**`static void resetOrientation()`**: Undoes every 90-degree turn and flip in one undo step: flips back if flipped, then rotates left until the turn count is 0, remapping the crop at each step so it stays on the same part of the picture. The straighten angle is kept. No-op if the orientation is already the identity.

**`static void setAngleFromSlider(float deg)`**: Straighten slider, per drag event: records `snapAngle(deg)` with `coalesce = true` (one undo step per drag). The resample is deferred to the canvas's dirty poll, once per frame.

**`static void setAngleDiscrete(float deg)`**: A discrete angle change (nudge button, reset): `snapAngle(deg)` with its own undo step, resampled at full resolution on the next frame (`markAdjustDirty`).

**`static void nudgeAngle(float delta)`**: Adds `delta` degrees to the current angle through `setAngleDiscrete`.

**`static void setCornerMode(bool expand)`**: **Input:** `expand` (true = grow the canvas so the corners stay, false = crop to the largest inner rectangle). Stores it via `adjust.setExpand` and asks for a recompute. No-op without a picture.

**`static void updateAnglePreview()`**: Interactive path for an angle drag on a large picture: turns the small `orientedPreview` by the current angle (`rotateArbitrary`), renders the adjustments over it into `state.previewImage`, and sets the preview document size (`previewDocW/H`) to `rotatedDims` of the full-resolution picture, so the preview is drawn stretched to the size the final result will have. The full-resolution turn runs once, when the drag ends.

**`static void undo()`**: Undoes one step of `state.adjust` (if there is one), drops the curve editors' selected points (the curves may be different ones now) and refreshes the picture.

**`static void redo()`**: Redoes one step; same follow-up as `undo()`.

## Opening, segmentation and saving

### `decode.hpp`
Opening a file without freezing the window. Decoding a big PNG or JPEG takes long enough to be noticed, so it runs on its own thread (`DecodeWorker`, a `LatestWorker` plus the bookkeeping of which path is being decoded). Same contract as the render and geometry workers: `submit()` supersedes any earlier request, the finished result is parked until the UI thread `collect()`s it, and the notify callback wakes the sleeping event loop. Until the result lands the previously open picture stays fully usable; nothing about the editor changes until `adoptDecoded()` (`meta_commands.hpp`) swaps the new file in. A decode that **throws** (`std::bad_alloc` on a huge picture, a library exception on a corrupt one) must not become an exception on the UI thread (`LatestWorker` would park it and rethrow it from `collect()`, where the open-file code expects an ordinary "couldn't open it"), so the job catches it and reports it as an empty `image` plus `error`; the previously open picture stays open. A decode already running can't be interrupted (the image libraries have no cancel hook), so opening a second file queues behind it and the first file's result is dropped when it finishes.

**Depends on:** engine: `image16.hpp`, `image_meta.hpp`, `latest_worker.hpp`, `virtual_copy.hpp`. **Test:** the worker protocol is the engine's `LatestWorker`, tested in `tests/cases/latest_worker.inc`; this file's own code has no case of its own. It is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `struct DecodeJob`: The path to open (a virtual copy's `photo.jpg|vc2` form is accepted).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `path` | `std::string` |  |  |

- `struct DecodeResult`: What a decode produced: the path, the 8-bit image (empty on failure; for a 16-bit file its 8-bit form), the library's error text, and `wide`, the full-precision 16-bit data (null for every other file).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `path` | `std::string` |  |  |
  | `image` | `std::optional<CanvasImage>` |  | empty on failure (for a 16-bit file: its 8-bit form) |
  | `error` | `std::string` |  | library message when image is empty |
  | `wide` | `std::shared_ptr<Image16>` |  | a 16-bit PNG at full precision (upright); null for every other file |

- `class DecodeWorker`: `LatestWorker<DecodeJob, DecodeResult>` plus the UI-thread bookkeeping of which file is being decoded.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `pendingPath_` | `std::string` |  | UI thread only |

  - `DecodeWorker() : LatestWorker<DecodeJob, DecodeResult>(runDecodeJob)`: Constructor: passes `runDecodeJob` to `LatestWorker`.
  - `void submit(std::string path)`: Remembers `path` as the pending path and submits a `DecodeJob`, superseding any earlier request.
  - `const std::string &pendingPath() const`: The file the newest request is decoding (UI thread; valid while outstanding()).

**State and tables**

- `decodeWorker`: `static DecodeWorker`

**Functions**

**`static std::optional<DecodeResult> runDecodeJob(DecodeJob &&job, WorkerCancel)`**: **Worker thread only.** Always returns a value, and never throws: a failure is an empty `image` plus an `error` string, because a decode can't be cancelled part-way. Everything runs inside a `try`: `std::bad_alloc` gives *"not enough memory to open this picture"*, any other `std::exception` gives its `what()` (*"unexpected error"* if that is empty), anything else gives *"unexpected error"*; `image` and `wide` are reset in each case. A file that opens but holds no picture with no message gives *"couldn't open it"*. Decodes `vcImageOf(path)` (a virtual copy decodes its parent picture) with `image_source::openUpright`, which applies the camera's EXIF rotation once, so the picture is upright everywhere afterwards (editor, masks, saved edits, export). A 16-bit PNG is read at full precision into `wide`; its 8-bit form (what the tools measure) is made from it.

### `segmentation.hpp`
Object segmentation for the Masks pane's Object tool, run in the background. The engine is MobileSAM (an ONNX encoder and decoder) when the model files can be loaded, with the colour-grow wand of the engine's `seg_basic.hpp` as the fallback. Holds the request / result types, `SegmentWorker`, the engine-info string the Object panel shows (the one global that a worker thread writes and the UI thread reads, behind a mutex), model-folder lookup, `segmentObject()` itself (worker thread only) and the UI-thread functions that take a finished result and put it into the mask as one undo step. Also ends with three forward declarations of filmstrip functions that the code after this file needs (see the note on that list).

**Depends on:** `filmstrip.hpp` (defined later); engine: `adjustment_set.hpp`, `latest_worker.hpp`, `mask_object.hpp`, `photo_meta.hpp`, `sam_segmenter.hpp`, `seg_basic.hpp`, `thumb_worker.hpp`. **Test:** the worker protocol is the engine's `LatestWorker`, tested in `tests/cases/latest_worker.inc`; this file's own code has no case of its own. It is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `using SegmentWorker = LatestWorker<SegmentRequest, SegmentResult>`: `LatestWorker<SegmentRequest, SegmentResult>`, the same protocol as the render and decode workers.
- `struct SegmentRequest`: One segmentation request: the untouched original, its key (`state.imageSerial`), the prompts, where the result goes (`maskIdx`, `part`), and `prefetch` (encode only, to warm the model; no result map).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `image` | `std::shared_ptr<const CanvasImage>` |  | the untouched original |
  | `imageKey` | `uint64_t` | `0` | state.imageSerial |
  | `prompts` | `std::vector<ObjectPrompt>` |  | original-picture fractions |
  | `maskIdx` | `int` | `-1` | where the result goes |
  | `part` | `int` | `-1` |  |
  | `prefetch` | `bool` | `false` | encode only (warm the model), no result map |

- `struct SegmentResult`: The request (image dropped; prompts and target kept), the soft map (null on failure) and the error text.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `req` | `SegmentRequest` |  | image dropped; prompts + target kept |
  | `map` | `std::shared_ptr<const ObjectMap>` |  | null on failure |
  | `error` | `std::string` |  |  |


**State and tables**

- `gSegInfoMutex`: `static std::mutex`. seg_basic.hpp as fallback) ---- Which engine is active, shown in the Object panel (written by the worker, read by the UI thread's label poll).
- `gSegInfo`: `static std::string`, initially `"Engine: loads on first use"`
- `segmentWorker`: `static SegmentWorker`, initially `{runSegmentJob}`

**Forward declarations** (defined in a later file; declared here because this code is written before it)

These three are filmstrip functions (`filmstrip.hpp`) needed by `adoptDecoded()` and the code before the filmstrip is defined. They sit at the end of this file because that is where the original code declared them; a leftover comment above them ("Swaps a finished decode into the editor") describes `adoptDecoded()` in `meta_commands.hpp`, not these declarations.

- `static void filmMarkEdited(const std::string &path)`
- `static int filmViewPos(int i)`
- `static void filmApplyMeta(const std::string &path, const PhotoMeta &m)`

**Functions**

**`static void setSegInfo(std::string s)`**: Replaces the engine-info text under the mutex. Called by the worker when the engine is chosen.

**`static std::string getSegInfo()`**: Returns a copy of the engine-info text under the mutex. Read by the Object panel's label poll on the UI thread. Starts as "Engine: loads on first use".

**`static std::string samModelDir()`**: **Returns** the folder holding `mobile_sam.encoder.onnx` and `mobile_sam.decoder.onnx`: `$LITEROOM_MODELS` if set and non-empty, else a `models` folder next to the executable (found through `GetModuleFileNameW` on Windows, `/proc/self/exe` elsewhere), else the relative path `models`.

**`static std::shared_ptr<const ObjectMap> segmentObject(const CanvasImage &img, uint64_t key, const std::vector<ObjectPrompt> &prompts, std::string *err, const std::atomic<uint64_t> *latest, uint64_t gen)`**
**Worker thread only.** **Inputs:** the untouched original image, the `key` identifying it (`state.imageSerial`), the click prompts (original-picture fractions, foreground or background), an error out-string, and the cancel pair `latest` / `gen`. **Returns** the soft `ObjectMap`, or null (no error for a prefetch; the text in `*err` on failure).
- On first use, tries to load the two SAM models from `samModelDir()`. A load that **throws** (the ONNX runtime reports a bad or missing model that way) counts the same as one that returns false, with the reason in the label; otherwise `backend` would stay untried and every later job would retry the load. Success: engine "MobileSAM". Failure: engine "basic color grow (<reason>; looked in <dir>)" and every later call uses `segmentObjectWand`. The engine and the segmenter are function-local statics, so only this one worker thread may call it.
- With SAM: encodes the image once per `key` (the expensive step; remembered in a local `encodedKey`, which is cleared **before** the encode and set only after it succeeds, because a throw or failure part-way leaves the encoder in an unknown state and a stale key would let the next job for the previous picture skip the encode). With no prompts it stops there: that is the **prefetch** that warms the model when the Object tool is opened. Otherwise it decodes the prompts into mask logits and turns them into 8-bit alpha with a sigmoid (`kSamSoftness` sets the edge softness), tagged with a fresh `nextObjectMapId()`. Abandons (returns null) if superseded while encoding.

**`static std::optional<SegmentResult> runSegmentJob(SegmentRequest &&req, WorkerCancel c)`**: **Worker thread only.** Runs `segmentObject` for one request and returns the `SegmentResult` (request without its image, plus the map or an error). Always returns a value, and never throws: failure is a null `map` and an `error`. A request with no image gives *"no picture to analyze"*; an exception from the segmenter is caught here (`std::bad_alloc` gives *"not enough memory"*, another `std::exception` its `what()`, anything else *"unexpected error"*) and shown by `adoptSegment` as "Object selection failed: ..." ("... unavailable: ..." for a prefetch). Without this, `LatestWorker` would park the exception and rethrow it from `collect()` on the UI thread, where nothing catches it.

**`static void adoptSegment(SegmentWorker::Result &&r)`**: **UI thread.** Takes a finished segmentation. **Prefetch results** just record `state.mask.encodedKey` (or show "Object selection unavailable: <error>"). Otherwise it clears the pending prompts, and on failure shows "Object selection failed". On success it stores the map and the prompts in the target mask part (an `ObjectShape`) with **one** undo step (`adjust.setMasks`), clears the status and asks for a recompute. A result whose mask or part is gone (deleted, undone away) or is no longer an object part is dropped silently.

**`static void collectSegmentJob()`**: Non-blocking: adopts a finished segmentation if there is one. Polled once per frame from `flushPendingAdjust`.

### `saved_edits.hpp`
Saving a picture's edits to its sidecar file, plus the per-picture facts the Info pane and the status bar show (camera data, colour-space tag). A photo's edits live in `photo.jpg.literoom.json` beside it. They are written (to a temp file, then renamed, so a crash can't leave half a file) after any change once no mouse drag is in progress (`autosaveSidecar`, polled every frame from `flushPendingAdjust`), when another picture replaces this one, and when the app closes; and read back when the picture is opened (`loadSidecar()`, which lives in `lut_pane.hpp` because it also restores the LUT reference). A missing file is normal. A damaged one, one from a newer version, or one written for a different-sized picture is ignored (the editor starts clean) and the status bar says so, but it is first copied to `<sidecar>.bak` so the next autosave can't destroy it. A file that exists but can't be opened or read at all sets `state.sidecarBlocked`, and then nothing writes that picture's sidecar until it is opened again (both in `loadSidecar()`). Opening a picture and touching nothing writes nothing. The undo history is not saved. The file format is in [sidecar-format.md](sidecar-format.md).

**Depends on:** `filmstrip.hpp` (defined later); engine: `adjustment_set.hpp`, `color_tag.hpp`, `exif.hpp`, `file_utils.hpp`, `image_meta.hpp`, `sidecar.hpp`, `virtual_copy.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static std::filesystem::path sidecarPath(const std::string &imagePath)`**: **Input:** a picture path (a virtual copy's `photo.jpg|vcN` form is accepted). **Returns** the path of its sidecar file as a `std::filesystem::path` (`sidecarFileOf`): `photo.jpg.literoom.json` beside the picture; a virtual copy has its own file (see `virtual_copy.hpp`).

**`static std::string pictureLabel(const std::string &path)`**: A picture's name for messages and the title bar: a virtual copy shows as "photo.jpg [Copy 2]".

**`static const exif::PhotoInfo &currentPhotoInfo()`**: The open picture's camera data for the Info pane (make, model, lens, ISO, shutter, ...), read from the file once per opened picture: the first time the pane asks after imageSerial changed. It reads the first 256 KiB of the file on the UI thread, which is a fraction of a millisecond; nothing here touches the render pipeline. A picture with no readable camera data (a PNG, a JPEG without EXIF) gives an empty PhotoInfo.

**`static const color_tag::ColorTag &currentColorTag()`**: What the open file says its color space is, for the Info pane and the status bar: read once per opened picture, like currentPhotoInfo (a PNG's color chunks, or a JPEG's ICC profile). The editor converts nothing: it edits the file's numbers as they are and carries the tag into PNG exports.

**`static void announceColorTag()`**: The status-bar notice for the file just opened: what it says its color space is, for every file (sRGB, untagged, or something else). A picture tagged as something other than sRGB also gets the reminder that nothing is converted. Short on purpose: setStatus cuts a message at 110 characters, and "Color: <profile> (embedded profile, not sRGB)" is already about 50 to 90 of them, so the part that matters comes first and the rest is brief.

**`static void saveSidecar()`**: Writes the sidecar for the open picture: `sidecarFileText(...)` of the file name, the original's size, `adjust.toJson()`, the rating / flag / label and the snapshots, to `<path>.tmp` and then renamed over the real file. Does nothing without a picture. Marks edits, meta and snapshots as saved *before* writing, so a failing disk isn't retried every frame, only on the next edit. **With `state.sidecarBlocked` set it writes nothing** and shows "Not saved: its saved edits couldn't be read. Reopen the picture" as an error (the change still counts as seen, so it is not retried every frame), because the unreadable file's content is unknown and would be replaced by this picture's possibly empty edits. A write error shows "Couldn't save edits ..." in the status bar. When the edits (not just a rating) changed, calls `filmMarkEdited` so the filmstrip shows its "has edits" dot.

**`static void flushSidecar()`**: Saves now if anything changed since the last save or load (edits, rating / flag / label, or snapshots). Called before another picture replaces this one and when the app closes.

**`static void autosaveSidecar()`**: Per frame: saves a finished change, but not while the mouse is still dragging a slider or a mask, because that would write on every frame.

## LUTs, Sync Settings and batch export

### `lut_pane.hpp`
The UI side of the LUTs pane: the library's `.cube` files as a list, each with a preview of the open picture under that LUT. The picture's own LUT is part of its edits (`AdjustmentSet::lut()`, with LUT Amount as its strength); everything here is the UI around it. The library and its cache are the engine's `io/lut_library.hpp`; the previews are made by `workers/lut_preview_worker.hpp`. The list and its bookkeeping are the **LUT globals** (`gLuts`, `gLutsLoaded`, `gLutPaintCounter`, `lutPreviewWorker`, `gLutSerial`, `gLutBuilt`, `gLutCacheEpoch`), left out of the `uiState` refactor on purpose, because `tests/lut_pane_test.cpp` drives this pane's functions. The file also holds `loadSidecar()`, which restores a picture's saved edits and re-checks its LUT, because it needs `dropLutCache()` and `noteLutProblem()`.

**Depends on:** `saved_edits.hpp`; engine: `adjustment_set.hpp`, `adjustments.hpp`, `lut3d.hpp`, `lut_library.hpp`, `lut_preview_worker.hpp`, `minijson.hpp`, `photo_meta.hpp`, `sidecar.hpp`. **Test:** `tests/lut_pane_test.cpp` drives this pane's functions (refresh, import, apply, remove, status texts) against a real temporary library (see [headers.md](headers.md)); the file is also compiled into `golden_render_test`.

**Types**

- `struct LutRow`: One row of the LUTs list: the library entry, the preview of the open picture under that LUT (null until made), the error text when the file can't be read, the warning ("may expect camera-log input"), and `paintVersion`, which changes whenever anything the row shows changes so the list canvas repaints it.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `entry` | `LutEntry` |  | name + path (hash and size arrive with the preview) |
  | `thumb` | `std::shared_ptr<const CanvasImage>` |  | null until made, or with no picture open |
  | `error` | `std::string` |  | why the file can't be used; "" if it can |
  | `warning` | `std::string` |  | "may expect camera-log input"; "" if not |
  | `paintVersion` | `uint64_t` | `0` | changes whenever what the row shows changes |

- `struct LutBuilt`: What the previews were made for: the picture's pixels and the edit state. A change of either makes them stale. The LUTs pane's own changes (choosing a LUT, its amount) do not, because a preview shows every LUT on the picture *without* the one it has now (see `lutPreviewsFollow`).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `valid` | `bool` | `false` |  |
  | `source` | `const CanvasImage *` | `nullptr` |  |
  | `version` | `uint64_t` | `0` |  |


**State and tables**

- `gLuts`: `static std::vector<LutRow>`. The rows of the LUTs list, at most `kLutRows`.
- `gLutsLoaded`: `static bool`, initially `false`. The folder was listed since the pane was last opened.
- `gLutPaintCounter`: `static uint64_t`, initially `0`. Counter stamping each row's `paintVersion` whenever its preview or text changes.
- `lutPreviewWorker`: `static LutPreviewWorker`. The worker that renders the previews (`LutPreviewWorker`, `workers/lut_preview_worker.hpp`).
- `gLutSerial`: `static uint64_t`, initially `0`. The serial of the batch whose results are taken in; results of an earlier batch are ignored.
- `gLutBuilt`: `static LutBuilt`. What the current previews were made for.
- `gLutCacheEpoch`: `static uint64_t`, initially `1`. Bumped when the LUT cache is emptied; `currentLutStatus()` looks up again when it changes.

**Functions**

**`static void dropLutCache()`**: Empties the library's cache (`clearLutCache()`) and bumps `gLutCacheEpoch`, so `currentLutStatus()` looks again. For when a LUT file may have changed: an import, or another picture being opened.

**`static void refreshLuts()`**: Lists the library folder (`listLutEntries(lutDir())`) afresh into `gLuts`, at most `kLutRows` rows. A row for a file that is still there keeps what is known about it (preview, hash, size, warning and paint version). If the set of paths changed (a file came or went), the preview worker is cancelled and `gLutBuilt` is invalidated so the previews are made again; if only the contents of rows changed they are not. Sets `gLutsLoaded` and repaints.

**`static LutStatus currentLutStatus()`**: **Returns** the `LutStatus` of the open picture's LUT in the library (none, missing, unreadable, or changed since it was chosen), from `adjust.lutStatus()`. Cached in function statics and looked up again only when `adjust.version()` or `gLutCacheEpoch` changed, because the pane's text asks every frame.

**`static bool noteLutProblem()`**: Shows a status-bar message when the picture's LUT can't be used as chosen: *not in the LUT library* (the picture shows without it), *can't be read* (shows without it), or *has changed since it was chosen* (shown as a notice, not an error). **Returns** true if it said something.

**`static std::string lutReason(const std::string &error)`**: Turns `"file.cube: line 3: ..."` into `"line 3: ..."` (the row already names the file); returns the text unchanged when it has no `": "`.

**`static bool lutPreviewsCurrent()`**: True when the previews match the picture and its edits right now: `gLutBuilt` is valid, was made for the same `state.source` and for the same `adjust.version()`.

**`static void lutPreviewsFollow(bool wasCurrent)`**: Call after a change made by the LUTs pane itself, passing what `lutPreviewsCurrent()` was just before it. The previews do not depend on that change, so if they were current, `gLutBuilt.version` moves to the new version and they stay current. An unrelated edit made in between keeps them stale.

**`static void setLutAmount(float v)`**: The LUT Amount slider's setter: sets `AdjId::LutAmount` and keeps the previews current, because they show every LUT at 100% whatever the amount is.

**`static void pollLutPreviews()`**: Once per frame (from `flushPendingAdjust`). First takes in the previews the worker has finished: results whose `serial` isn't `gLutSerial` (a replaced batch) are skipped; the rest are copied into the matching row (hash, size, thumbnail, error, warning) with a new paint version. Then, only with the LUTs pane open: lists the folder if it hasn't been (`gLutsLoaded`), and — with a picture open, no drag, and a non-empty list — starts a new batch when the previews are not current: if the picture itself changed, every row's old thumbnail is cleared first; the job is `{serial, base = previewBase or source, buildRenderParams(), every row's entry}`; `gLutBuilt` records the source and version it was made for.

**`static void applyLutRow(size_t i)`**: **Input:** a list row index. Gives the picture that row's LUT: one undo step, "LUT: <name>" in History. Ignored while dragging, with no picture, or for an out-of-range row. A row whose file can't be read shows "Can't use <name>: <reason>". The hash stored in the edit is the file's own, as the library reads it now (`acquireLut`). If `setLut` reports no change it says "Already using the LUT <name>"; otherwise the previews follow, the picture is recomputed and the status says "Applied LUT: <name>".

**`static void removeLut()`**: Takes the LUT off the picture (and its amount back to 0): one undo step, "Remove LUT". Ignored while dragging or with no picture; does nothing if there was no LUT.

**`static void importLutPaths(const std::vector<std::string> &files)`**: **Input:** paths of `.cube` files. Copies each into the library with `importLutFile(file, lutDir(), ...)`, then drops the cache and re-lists. Status: "Couldn't import: <first error>" if none worked; a warning that the (first) imported LUT "may expect camera-log input and look wrong on this picture" (imported, not refused); otherwise "Imported LUT: <name>" or "Imported N LUTs", with "(M skipped: <first error>)" when some failed.

**`static void importLuts()`**: Opens the file picker ("Import LUTs", filters `*.cube` and all files) and passes the result to `importLutPaths`.

**`static std::string lutRowDetail(size_t i)`**: **Input:** a row index. **Returns** the line under the row's name: "Can't be read: <reason>", or the table size as "N³ points" plus "may expect camera-log input" when the file has that warning (joined with a bullet). Empty for an out-of-range row.

**`static std::string lutCurrentText()`**: **Returns** what the pane says about the picture's own LUT: "No LUT on this picture", "LUT: <name>", or the same with the problem named (not in the library / can't be read, "so it has no effect"; or "the file has changed since it was chosen").

**`static bool backUpIgnoredSidecar(const std::filesystem::path &sidecar)`**: Copies a saved-edits file that is about to be ignored to `<sidecar>.bak`, so the edits the next autosave writes can't destroy what was in it. An existing `.bak` is left alone (the first copy is the one worth keeping). **Returns** true if the copy exists afterwards.

**`static void loadSidecar()`**: Applies the picture's saved edits, if it has any. Call right after `adjust.reset()` + `setOriginalSize()` and **before** `syncSource()`, so the saved crop / rotation is what gets built. Starts from a pristine state (`sidecarSaved = adjust.version()`, no rating, no snapshots, `sidecarBlocked` false, LUT cache dropped). No file is the normal case and returns silently. A file it can't use must not be lost to the next autosave, so there are two kinds:
- **Unreadable** (the file is there but can't be checked, opened or read, or its size is negative or over 64 MiB): the picture opens with `state.sidecarBlocked` set and the status "Changes won't be saved: saved edits unreadable (<why>)" as an error, where <why> is one of *couldn't check the file*, *couldn't open the file*, *unexpected file size*, *couldn't read the file*. Content unknown and not copyable, so `saveSidecar` refuses to write until the picture is opened again.
- **Unusable** (read, but not applicable): JSON that doesn't parse, a `version` newer than `kSidecarVersion` ("made by a newer Literoom"), an `image` size different from the picture's ("the picture's size has changed"), or edits that `adjust.loadJson` rejects ("unreadable"). The file is first copied by `backUpIgnoredSidecar`, the picture opens clean and the status says "Ignored saved edits: <why> (old file kept as .bak)". If even the copy fails, it counts as unreadable (blocked, "can't back up the old file").
- An **empty** file (size 0) holds nothing to lose: "Ignored saved edits: the file is empty", not blocked.
On success it adopts the edits, the rating / flag / label and the snapshots, says "Restored saved edits" unless the file only holds a rating, and calls `noteLutProblem()`.

### `sync.hpp`
**Sync Settings** and **batch rating**: copying the open picture's look onto the pictures picked in the filmstrip (Ctrl+click, Shift+click). The other pictures are not open, so there is no `AdjustmentSet` to edit: their sidecar files are rewritten directly by the engine's `sidecarSyncLook`, which keeps each picture's own crop, rotation and masks. A picture with no sidecar has to be decoded once to learn its size. All of that runs on a background worker. It is a `LatestWorker` but is never cancelled: a second sync is refused while one runs, and opening pictures waits too, so nobody reads a sidecar that is about to be replaced. The open picture is never in the batch (its edits live in memory and autosave). The same worker does the **batch rating**: when pictures are selected, a rating / flag / colour-label command is applied to the open picture at once and to the others through `metaOnly` jobs that patch only the rating fields (`sidecarPatchMeta`).

**Depends on:** `saved_edits.hpp`, `filmstrip.hpp` (defined later); engine: `adjustment_set.hpp`, `file_utils.hpp`, `image_meta.hpp`, `latest_worker.hpp`, `photo_meta.hpp`, `preset.hpp`, `sidecar.hpp`, `virtual_copy.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `struct SyncJob`: One batch: the pictures to touch, the look to copy (`Preset`), or, with `metaOnly`, a rating / flag / label `patch` to apply instead.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `paths` | `std::vector<std::string>` |  |  |
  | `look` | `Preset` |  | only its values, curves, mix, grade and LUT are used |
  | `metaOnly` | `bool` | `false` | rate / flag / label the pictures instead |
  | `patch` | `MetaPatch` |  | ... with this |

- `struct SyncOutcome`: The result for one picture.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `path` | `std::string` |  |  |
  | `ok` | `bool` | `false` |  |
  | `error` | `std::string` |  | why it was skipped |
  | `meta` | `PhotoMeta` |  | metaOnly: the picture's meta after the patch |

- `struct SyncResult`: The results of one batch, in the order of the job's paths.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `metaOnly` | `bool` | `false` |  |
  | `items` | `std::vector<SyncOutcome>` |  |  |


**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static LatestWorker<SyncJob, SyncResult> syncWorker(runSyncJob)`

**Functions**

**`static int readSidecarText(const std::string &imagePath, std::string *text)`**: 1 = text read, 0 = no sidecar, -1 = unreadable or too big. Any thread. "No sidecar" means the file is not there; a file that is there but can't be opened, or whose existence can't even be checked, is -1, never 0, because treating it as absent would rebuild the sidecar from nothing and rename it over the real file, losing that picture's crop, rotation and masks.

**`static bool writeTextAtomic(const std::filesystem::path &path, const std::string &text, std::string *err)`**: Temp file, then rename, like saveSidecar. Any thread.

**`static SyncOutcome syncOne(SyncJob &job, const std::string &path)`**: **Worker thread only.** One picture of a batch. May throw (`std::bad_alloc`, a library exception): `runSyncJob` is the boundary. A sidecar that exists but can't be read gives *"couldn't read its saved edits"*, and the picture is skipped. The steps are those listed under `runSyncJob`.

**`static std::optional<SyncResult> runSyncJob(SyncJob &&job, WorkerCancel)`**: **Worker thread only.** Always returns a value (it can't be cancelled), and never throws. Every picture is guarded on its own: one job is a whole batch and each finished picture is already written to disk, so an exception on picture 7 of 20 must not discard the outcomes of 1 to 6 (the UI would never mark them as edited). A throw becomes a skipped picture with a reason (`std::bad_alloc` gives *"ran out of memory"*, another `std::exception` its `what()`, anything else *"unexpected error"*) and the batch carries on. For each path (`syncOne`): read its sidecar text (`readSidecarText`); a missing sidecar with a `metaOnly` patch that would record nothing is counted as done without decoding. A missing sidecar otherwise makes it learn the upright size (`image_source::uprightSize`). Then builds the new text with `sidecarPatchMeta` (rating-only jobs) or `sidecarSyncLook` (look jobs: values, curves, Color Mixer, Color Grading, LUT) and writes it atomically. One `SyncOutcome` per picture says whether it worked and why not.

**`static bool syncBusy()`**: True while a sync is running, and then also shows "Wait for the sync to finish" in the status bar. Commands that touch sidecars (opening a picture, another sync) call this first.

**`static void collectSyncJob()`**: Polled once per frame from `flushPendingAdjust`. When a result is ready: for each success marks the strip (`filmApplyMeta` for a rating-only job, which sets no "has edits" dot; `filmMarkEdited` for a look sync) and counts failures, remembering the first as "<picture>: <reason>". Status: "Updated N selected photo(s)" or "Synced settings to N photo(s)", or the same with ", skipped M (<first error>)" as an error.

**`static void syncSettings()`**: Image > Sync Settings to Selected. Ignored while dragging, with no picture open, or while a sync runs. Collects the selected pictures other than the open one that the filter isn't hiding. With none: "Select other photos in the filmstrip first (Ctrl+click, Shift+click)". Otherwise snapshots the open picture's look (`adjust.currentPreset`), submits the job and shows "Syncing settings to N photo(s)...".

### `export.hpp`
**Batch export** from the filmstrip: File > Export Selected... and Export All Shown... write every picked picture to one folder with the Export pane's settings. Like Sync, the pictures are not open, so the work is a pure function of each picture's file and its sidecar (`exportPicture` in `workers/export_worker.hpp`: decode, load the edits into a local `AdjustmentSet`, orient → straighten → crop → render, resize, encode). The open picture is exported from its sidecar too, so its pending edits are flushed to disk first. Unlike Sync it can be stopped: Stop Export drops what is queued and abandons the picture being rendered. Export only *reads* sidecars, so it does not block opening pictures or editing; a picture is exported with the edits it has on disk when the batch reaches it. (The single-picture export, `exportImage`, is in `meta_commands.hpp`.)

**Depends on:** `saved_edits.hpp`; engine: `export_worker.hpp`, `file_utils.hpp`. **Test:** the work a batch does (`exportPicture`, `ExportWorker`) is tested in the engine: `tests/cases/export_worker.inc`, `export_render.inc`. This file's own code has no case of its own; it is compiled into `golden_render_test` and `lut_pane_test`.

**Types**

- `struct ExportRun`: Progress of the running batch export (UI thread only): whether one is `active`, the `total`, the counts `ok` / `bad` / `cancelled`, `shown` (the handled count last put in the status bar), the first error and the destination folder.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `active` | `bool` | `false` |  |
  | `total` | `size_t` | `0` |  |
  | `ok` | `size_t` | `0` |  |
  | `bad` | `size_t` | `0` |  |
  | `cancelled` | `size_t` | `0` |  |
  | `shown` | `size_t` | `0` |  |
  | `firstErr` | `std::string` |  |  |
  | `folder` | `std::string` |  |  |


**State and tables**

- `exportWorker`: `static ExportWorker`. The worker that exports the batch (`ExportWorker`, `workers/export_worker.hpp`).
- `exportRun`: `static ExportRun`. The running batch's progress.

**Functions**

**`static bool exportBusy()`**: True while a batch export is running, and then also shows "Wait for the export to finish (or use Stop Export)".

**`static std::vector<std::string> exportTargets(bool selectedOnly)`**: **Input:** `selectedOnly`. **Returns** the paths of the strip's pictures in strip order, as the filter shows them: only the selected ones, or all of them.

**`static void startExportBatch(std::vector<std::string> paths)`**: **Input:** the pictures to export. Ignored while dragging or while an export runs. With none: "Select pictures in the filmstrip first (Ctrl+click, Shift+click), or use Export All Shown". Asks for a folder ("Export to Folder"; cancel = stop), flushes the open picture's sidecar, builds an `ExportSettings` from the Export pane's fields (`exportJpeg`, `exportPng16` only when not JPEG, `jpegQuality`, `keepMetadata`, `removeGps` only with `keepMetadata`, `exportResize`, `exportLongEdge`, `exportOverwrite`), starts the worker and resets `exportRun`. Status: "Exporting N photo(s)...".

**`static void exportSelected()`**: File > Export Selected...: `startExportBatch(exportTargets(true))`.

**`static void exportAllShown()`**: File > Export All Shown...: `startExportBatch(exportTargets(false))`.

**`static void stopExport()`**: Stop Export: cancels the worker (it drops what is queued and abandons the picture being rendered) and shows "Stopping the export...". No-op when nothing is running.

**`static void collectExportJob()`**: Polled once per frame from `flushPendingAdjust`. Does nothing without an active run. Takes the outcomes so far (asking `busy()` *before* collecting, so a finished worker's last outcomes aren't lost) and counts ok / cancelled / failed, keeping the first error. While running it shows "Exporting X of N...". When finished it ends the run with "Exported N photo(s) to <folder>", "Exported N, skipped M (<first error>)", or, if stopped, "Export stopped: N exported, M skipped, K not exported".

## Filmstrip and per-picture commands

### `filmstrip.hpp`
The filmstrip: a row of thumbnails along the bottom of the window listing the pictures of the open picture's folder, with a filter bar on top. Holds the strip's logic (listing a folder into a `FilmStrip`, the filter, selection, scrolling, keeping the strip in step with the open picture, opening a picture by index), the thumbnail bookkeeping (queueing the `ThumbWorker`, taking its results in, one GPU texture per thumbnail) and the painting. The data types (`FilmEntry`, `FilmStrip`) are in `editor_state.hpp`; the thumbnail maker and worker are the engine's `workers/thumb_worker.hpp`. UI thread only, except the worker. Files shown are the folder's JPEG / PNG pictures plus their virtual copies (`photo.jpg|vc2`), at most `kFilmMaxFiles`. The `// BEGIN filmstrip` / `// END filmstrip` fence is kept around the whole file.

**Depends on:** `decode.hpp`, `sync.hpp`, `meta_commands.hpp` (defined later); engine: `file_utils.hpp`, `image_meta.hpp`, `photo_meta.hpp`, `thumb_worker.hpp`, `virtual_copy.hpp`. **Test:** the thumbnail maker and worker are tested in the engine (`tests/cases/thumb_worker.inc`). This file's own code has no case of its own; it is compiled into `golden_render_test` and `lut_pane_test`.

**Constants**

A strip under the canvas with a thumbnail of every JPEG / PNG in the open picture's folder. Click one (or press Left / Right) to open it; a dot marks pictures that have saved edits. It is one canvas that paints only the cells in view, so a folder of thousands costs nothing extra. Thumbnails are made by a background thread, nearest-to-the-open-picture first, and the open picture's thumbnail is refreshed (edits included) when you leave it.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kFilmBarH` | `float` | `26.0f` | the filter bar on top of the cells |
| `kFilmCellsH` | `float` | `104.0f` | the row of thumbnails below it |
| `kFilmH` | `float` | `kFilmBarH + kFilmCellsH` | Whole strip height: bar + cells |
| `kFilmCellW` | `float` | `108.0f` | Width of one thumbnail cell |
| `kFilmMaxFiles` | `size_t` | `5000` | a bigger folder shows its first 5000 |

Five stars ("at least N", click the lit one again to switch it off), Pick and Reject, five color labels, and "Clear" while a filter is on. Painting and clicking both read the same list, so they can't disagree.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kBarPad` | `float` | `8.0f` | Left padding of the filter bar |
| `kBarBtn` | `float` | `22.0f` | Width of a star / label button |
| `kBarGap` | `float` | `14.0f` | Gap between button groups |
| `kBarPickW` | `float` | `38.0f` | Width of the Pick button |
| `kBarRejectW` | `float` | `52.0f` | Width of the Reject button |
| `kBarClearW` | `float` | `44.0f` | Width of the Clear button |
| `kLabelDisplay[kLabelCount]` | `char *const` | `{"Red", "Yellow", "Green", "Blue", "Purple"}` |  |

**Types**

- `using VcMap = std::map<std::string, std::vector<int>>`: A folder's virtual copies: lower-case picture name → the copy numbers.
- `struct FilmBarItem`: One button of the filter bar: its `kind` (`None`, `Star`, `Pick`, `Reject`, `Label`, `Clear`), its number `n` (star 1..5 or label 1..5) and its x position and width.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `kind` | `Kind` | `None` |  |
  | `n` | `int` | `0` | the star count or the label number |
  | `x` | `float` | `0.0f` |  |
  | `w` | `float` | `0.0f` |  |

  - `enum class Kind: `None`, `Star`, `Pick`, `Reject`, `Label`, `Clear``

**State and tables**

- `thumbWorker`: `static ThumbWorker`, initially `{ [](const std::string &path) { return exifOrientationOf(path); }}`. The worker that makes the thumbnails (`ThumbWorker`, `workers/thumb_worker.hpp`).
- `kLabelColors`: `constexpr std::array<Color, kLabelCount>`, initially `{ Color{225, 70, 70}, Color{232, 196, 60}, Color{80, 185, 100}, Color…`. The five colour labels' colours (Red, Yellow, Green, Blue, Purple).

**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static void adoptDecoded(DecodeWorker::Result &&r)`: defined below

**Functions**

**`static void filmPushEntry(FilmStrip &f, const std::string &path, const std::string &name, int copy)`**: Appends one `FilmEntry` (path, display name, copy number) to the strip and records it in the name index `byName`. The index is keyed by the lower-cased last part of the path, so a virtual copy (`photo.jpg|vc2`) never collides with its picture.

**`static void filmPushWithCopies(FilmStrip &f, const std::string &path, const std::string &name, const VcMap &copies)`**: Appends a picture and then its virtual copies, taken from `copies` (the folder's copies by lower-case name, from `listVirtualCopies`).

**`static FilmStrip filmListFolder(const std::string &folder)`**: **Input:** a folder path. **Returns** a new `FilmStrip` (not yet shown) for it: the folder's JPEG / PNG files in listing order (`listImageFiles`, at most `kFilmMaxFiles`), each followed by its virtual copies.

**`static void filmRebuildView()`**: Recomputes `view`, the indices of the files the strip shows: all of them with no filter, else the ones the filter matches. The open picture (`current`) and the one being opened (`target`) always stay in, so rating the picture you are looking at under a filter can't make it vanish. Marks the strip dirty.

**`static int filmViewPos(int i)`**: **Input:** a file index. **Returns** its position in `view` (binary search), or -1 if it isn't shown.

**`static void filmClampScroll()`**: Keeps `scrollX` within `[0, view.size() × kFilmCellW − viewW]` (0 when everything fits).

**`static void filmEnsureVisible(int i)`**: Scrolls the strip the least amount that brings file `i`'s cell fully into view (using 800 before the first paint has set `viewW`). Does nothing if `i` isn't shown.

**`static void filmQueueThumbs()`**: Gives the thumbnail worker its queue: the pictures that still lack a thumbnail and haven't failed, ordered by distance from the target (or the open picture), alternating right then left, nearest first.

**`static void filmAdoptFiles(const std::vector<std::string> &paths)`**: **Input:** an already sorted list of paths from one folder. Replaces the strip with one holding exactly those pictures (and each one's virtual copies); the folder is taken from the first path. Keeps the strip's visibility, starts with no filter.

**`static bool filmAdoptFolder(const std::string &folder)`**: **Input:** a folder. Makes it the strip's (`filmListFolder`). **Returns** false, changing nothing, if it holds no JPEG / PNG pictures. Keeps visibility; a new strip has no filter.

**`static void filmSync(const std::string &imagePath)`**: **Input:** the path of the picture that is now open. Follows it: if it isn't in the current strip (or the strip is for another folder) the strip is rebuilt around it (`filmAdoptFiles({path})`); sets `current` and `target`; copies the editor's own rating / flag / label (loaded with its sidecar) into its entry; rebuilds the view, records `thumbVersion = adjust.version()`, scrolls to it and re-aims the thumbnail worker. Call after `loadSidecar()`.

**`static void filmLeave()`**: The open picture is about to be replaced: if it has been edited since its thumbnail was made (`adjust.version() != thumbVersion`), make its thumbnail from `state.image` so the strip shows the edits, and mark it edited.

**`static void filmMarkEdited(const std::string &path)`**: Called when a sidecar was written for `path`: sets that entry's "has edits" dot (once) and marks the strip dirty. Ignores a path not in the strip.

**`static void filmApplyMeta(const std::string &path, const PhotoMeta &m)`**: A batch rating finished for `path`: stores the new rating / flag / label in its entry (not for the open picture, whose value is the editor's own), re-filters if a filter is on (the picture may now appear or disappear) and marks the strip dirty.

**`static void collectThumbJobs()`**: UI thread: takes in the thumbnails the worker has made. Results from a folder we have left are ignored. A new thumbnail replaces the entry's, except the open picture's own once it has edits (its thumbnail already shows them); an original's thumbnail is shared with the virtual copies right after it that have none. A failed thumbnail marks the entry `failed` (a virtual copy is skipped on purpose, not a failure). The first reading of each entry's rating / edited flag is taken from the worker; after that the editor's own value is the truth. Re-filters if a filter is waiting for metadata that just arrived.

**`static void filmOpenIndex(int i)`**: **Input:** an index into `files`. Opens that picture. Ignored if out of range, already the target, while dragging or while a sync runs. Sets `target`, scrolls to it and keeps it in the view. If it is another set of edits (a virtual copy) of the picture already open (same `vcImageOf`), it skips the decode and adopts a copy of the open pixels exactly as a finished decode would (`adoptDecoded`, which flushes the old edits and loads these). Otherwise shows "Opening <name>..." and submits the path to `decodeWorker`.

**`static void filmStep(int d)`**: **Input:** +1 or −1 (Left / Right keys). Opens the previous / next picture among those the strip shows, relative to the one asked for last (so quick repeats keep moving). If the base picture is hidden by the filter it starts from the edge. Clamped at both ends.

**`static void filmScrollBy(float dx)`**: Scrolls the strip by `dx` logical pixels, clamped.

**`static void filmClearSelection()`**: Deselects every picture.

**`static void filmSelectAll()`**: Selects exactly the pictures the strip shows (Ctrl+A); hidden ones are deselected.

**`static std::vector<FilmBarItem> filmBarItems()`**: **Returns** the filter bar's buttons, left to right with their x and width: five stars (`kMaxRating`), a gap, Pick, Reject, a gap, the five colour labels, and Clear (after a gap) only while a filter is on. Painting and clicking both read this list, so they can't disagree.

**`static void filmFilterChanged()`**: The filter changed: rebuilds the view and deselects anything now hidden (Sync Settings would write into it unseen), clamps the scroll and keeps the open picture visible.

**`static void filmBarClick(float x)`**: **Input:** x in the filter bar. Finds the button under it and toggles the matching filter: the minimum rating ("at least N"; clicking the lit one again clears it), the Pick or Reject flag, a colour label, or Clear (resets the filter). Then `filmFilterChanged()`.

**`static void filmClick(float localX, float localY, const KeyModifiers &mods)`**: **Inputs:** a click in strip-local coordinates and the key modifiers. Above the cells it is a filter-bar click. On a picture: a plain click opens it (`filmOpenIndex`) and clears the selection. **Ctrl+click** toggles that picture in the selection; **Shift+click** selects the range from the anchor to it, among the pictures shown. The open picture joins the selection the first time, so it is part of a range.

**`static void toggleFilmstrip()`**: Shows or hides the strip (View menu / shortcut).

**`static bool filmstripShown()`**: True when the strip is switched on and holds at least one picture.

**`static void openFolder()`**: File > Open Folder: lets the user pick a folder, lists it, and opens its first picture. Does nothing if the folder has no JPEG / PNG pictures (the status bar says so).

**`static std::string starsText(int n)`**: **Input:** n. **Returns** n filled stars (★) as a UTF-8 string.

**`static const std::shared_ptr<CanvasTexture> &filmThumbTexture(const FilmEntry &e)`**: **Returns** the GPU texture `paintFilmstrip` draws for an entry: built from `e.thumb` the first time and again whenever `e.thumb` is replaced. Every thumbnail keeps its own copy because `drawImage` has one cache slot per canvas, so drawing a row of thumbnails with it re-uploaded every visible one on every repaint. The source image is held beside the texture so its address can't be reused while compared. Null if there is no thumbnail or the texture couldn't be made.

**`static void paintFilmstrip(CanvasContext &ctx)`**: The canvas painter for the whole strip. Records the canvas width as `viewW`, clamps the scroll, fills the background and returns if the strip is empty. Draws the **filter bar** (a lit background for an active Pick / Reject / label, stars filled up to the minimum rating, the Clear button) and then the **cells** that are in view: each thumbnail through `filmThumbTexture`, a highlight on the open picture and the selected pictures, and the rating stars, flag and colour label of the entry plus its "has edits" dot.

### `meta_commands.hpp`
Four groups of commands that don't belong to one pane. **Rating, flag and colour label** (`setPhotoMeta`, `applyMetaPatch`, `ratePhoto`, `flagPhoto`, `labelPhoto`): not edits, so they make no undo step and no History label and never touch `adjust.version()`. The open picture's value lives in `state.meta`, is mirrored into the strip's entry and is saved in the sidecar's `meta` object (`autosaveSidecar` / `flushSidecar` look at `state.metaDirty`). A command changes the open picture at once; if other pictures are selected in the filmstrip it changes them too, on the Sync Settings worker (their sidecars are patched in the background and the strip updates when the batch finishes). The open picture is never in that batch, so it can't race with autosave. "Choose the value it already has to clear it" is decided from the open picture, and the result (set or clear) is applied to every selected picture alike. **Opening** (`adoptDecoded`, `collectDecodeJob`, `openImage`): `adoptDecoded` is the one place where a freshly decoded picture replaces the open one. **Single-picture export** (`exportImage`). **`toggleSidebar`**.

**Depends on:** `render_worker.hpp`, `render_update.hpp`, `crop.hpp`, `geometry_worker.hpp`, `decode.hpp`, `segmentation.hpp`, `saved_edits.hpp`, `lut_pane.hpp`, `sync.hpp`, `filmstrip.hpp`; engine: `adjustment_set.hpp`, `color_tag.hpp`, `export_worker.hpp`, `file_utils.hpp`, `image16.hpp`, `image_codec.hpp`, `photo_meta.hpp`, `raster_ops.hpp`, `render_core.hpp`, `thumb_worker.hpp`, `virtual_copy.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void setPhotoMeta(const PhotoMeta &in, const std::string &message)`**: **Inputs:** the wanted `PhotoMeta` and the status message. Sanitizes it (`photoMetaSanitized`); returns at once if it equals `state.meta`. Otherwise stores it, sets `metaDirty` (so the sidecar is written), mirrors it into the open picture's strip entry, re-filters if a filter is on (the open picture stays in the strip; others may come or go) and shows the message.

**`static std::vector<std::string> metaBatchTargets()`**: **Returns** the selected pictures other than the open one that the strip shows (never one the filter hides): the pictures a batch rating applies to.

**`static void applyMetaPatch(const MetaPatch &patch, const std::string &message)`**: **Inputs:** a `MetaPatch` (the fields to change) and the message. With no picture open it says "Open a picture first". If other pictures are selected and a sync is already running it does nothing at all (one batch at a time). Otherwise it applies the patch to the open picture now (`setPhotoMeta`) and, if there are others, submits a `metaOnly` `SyncJob` for them and shows "<message> — updating N selected photo(s)...".

**`static void ratePhoto(int n)`**: **Input:** 0..5 (clamped to `kMaxRating`). Choosing the rating the open picture already has clears it (0). Message: "Rating: ★★★" or "Rating cleared".

**`static void flagPhoto(PhotoFlag flag)`**: **Input:** `PhotoFlag::Pick`, `Reject` or `None`. Pick and Reject toggle (choosing the current flag clears it); `None` clears. Messages: "Flagged as pick", "Flagged as rejected", "Flag cleared".

**`static void labelPhoto(int c)`**: **Input:** 1..`kLabelCount` toggles that colour (choosing the current one clears it); 0 clears. Messages: "Label: <colour>" or "Label cleared".

**`static void adoptDecoded(DecodeWorker::Result &&r)`**
**Input:** a finished decode (`DecodeWorker::Result`). The one place a newly decoded picture replaces the open one (also reused by a virtual copy of the open picture, with the open pixels copied).
- **Failure** (no image): shows "Couldn't open <picture>: <error>" and leaves whatever is open untouched (the strip's target returns to the current picture).
- **Success**, in this order: disarm the white-balance picker; `uiState.resetForNewPicture()`; clear the status; `flushSidecar()` (the picture being replaced keeps its latest edits); `filmLeave()` (its thumbnail shows them); invalidate the render, geometry and segmentation workers (anything in flight belongs to the previous picture) and the pending object prompts; bump `imageSerial`; install `original` / `original16` / `imagePath`; 100% zoom, scroll 0; `adjust.reset()` and `setOriginalSize`; no selected mask; `loadSidecar()` (before `syncSource`, so the saved crop and rotation are what gets built); `filmSync()`; cancel any drag; drop the old `image` (forces the synchronous path); `syncSource(true)`; `updateImageFromAdjustments()`; set `colorNoticePending` (a large picture's geometry job would otherwise wipe the notice when it lands) and `announceColorTag()`.

**`static void collectDecodeJob()`**: Non-blocking: adopts a finished decode if there is one. Polled once per frame from `flushPendingAdjust`.

**`static void openImage()`**: File > Open: shows the multi-select picker ("Open Images", JPEG / PNG, with "All Files" as an alternative). Does nothing while a sync runs or on cancel. With several files it drops anything that isn't `.jpg` / `.jpeg` / `.png` ("No JPEG or PNG pictures selected" if nothing is left) and sorts them in natural name order. The strip becomes exactly what was picked (`filmAdoptFiles`), thumbnails are queued and the first picture is opened through the normal background decode (`filmOpenIndex(0)`).

**`static void exportImage()`**: File > Export...: writes the picture as you see it (every edit, crop and rotation applied) to one file. Does nothing without a picture. First waits for any geometry job and render in flight (`finishGeometryJob`, `finishFullRender`) so a stale frame is never exported. Suggests `<name>-edit.jpg` / `.png` (a virtual copy adds `-copyN`); the Export pane's format only decides what the save dialog offers first, the **file name decides the format** (a name with no usable extension gets the pane's format). The output size follows Resize / Long edge (`fitLongEdge`, never enlarged); a picture JPEG can't hold (`kJpegMaxSide`) gets a message telling the user to turn on Resize or export as PNG. **16-bit PNG** re-renders the current source from its 16-bit twin (`renderToImage16`) with the same edits, rounded to 16 bits once at the end, then scales it the same way. A JPEG is written by `saveJpegFile` (quality, keep metadata, remove GPS, EXIF copied from the source file); an 8-bit PNG by `encodePng`. A PNG carries its source's colour tags (`color_tag::carryColorToPng`), and the status says so ("[sRGB kept]"); a non-sRGB PNG source exported to JPEG says the profile was not carried. Status: "Exported <file> [16-bit] [...] (W × H)" or "Export failed: <reason>".

**`static void toggleSidebar()`**: Opens the sidebar (activity 0) if it is closed, closes it if it is open (`activeActivity` -1 means closed).

## Widgets and colour controls

### `widgets.hpp`
Small reusable building blocks for the window: the slider row (`SliderSpec` + `buildSliderRow`, and `buildAdjustmentSlider` for one `AdjustmentSpec`), the titlebar and sidebar buttons, the dropdown menu button, the pointer helpers (`closeMenus`, `swallowPress`, `bindDrag`, `applyClickCursors`) and the registry that hands keys to a focused name field (`gTextFields`). Nothing here knows about a particular pane. The pointer-event rules these helpers work around are in `liteui.hpp` ("Pointer events"): a press runs the `onPointerDown` of every view in its chain, deepest first, until one calls `stopPropagation()`.

**Depends on:** `render_update.hpp`; engine: `adjustments.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

What a slider row needs to know: its label, range, how to read/write the value, and how to print it. Shared by the Adjust sliders (via buildAdjustmentSlider) and the Straighten angle slider. Export's "Resize" slider: the long side, in px, in steps of kExportLongEdgeStep.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kExportLongEdgeMin` | `int` | `128` | Export's "Resize" slider: smallest long side, in px |
| `kExportLongEdgeMax` | `int` | `8192` | Largest long side, in px |
| `kExportLongEdgeStep` | `int` | `16` | Slider step, in px |

**Types**

- `struct TextFieldKeys`: A registered name field: its shared `TextInputState` and the `onKeyDown` the `TextInput` was built with. A global shortcut is matched *before* the focused view's `onKeyDown`, and a match ends the dispatch, so a plain-key shortcut (Left, Right, Delete, Enter) would starve a focused text input of that key even when the shortcut stands down. Fields are registered here so those shortcuts can hand the key to the field.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `state` | `std::shared_ptr<TextInputState>` |  |  |
  | `onKeyDown` | `std::function<void(KeyEvent)>` |  |  |

- `struct MenuItem`: One row of a dropdown: label, action, and an optional shortcut hint shown right-aligned (display only; the real bindings are in `installShortcuts`).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `label` | `std::string` |  |  |
  | `action` | `std::function<void()>` |  |  |
  | `shortcut` | `std::string` | `{}` | optional |

- `struct SliderSpec`: What a slider row needs to know: its label, range, how to read and write the value, and how to print it. Shared by the Adjust sliders (via `buildAdjustmentSlider`) and the Straighten angle slider.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `name` | `std::string` |  |  |
  | `lo` | `float` | `0.0f` |  |
  | `hi` | `float` | `1.0f` |  |
  | `get` | `std::function<float()>` |  |  |
  | `set` | `std::function<void(float)>` |  | called with each new value while dragging |
  | `format` | `std::function<std::string(float)>` |  | value -> text after "name: " |
  | `isAngle` | `bool` | `false` | drag drives the angle preview instead of the pixel-adjustment preview (see canvasDirtySource) |
  | `affectsPixels` | `bool` | `true` | false: the value only changes a tool setting (brush size...), so a drag needs no re-render |
  | `hasAccent` | `bool` | `false` | tint the fill + thumb border (Color Mixer bands) |
  | `accent` | `Color` | `{}` |  |
  | `trackW` | `float` | `150.0f` | track length in px |
  | `onEnd` | `std::function<void()>` |  | optional: fired when a drag on it ends |


**State and tables**

- `gTextFields`: `static std::vector<TextFieldKeys>`. Every registered name field (see `registerTextField`); only ever appended to.

**Functions**

**`static void closeMenus()`**: Closes the open titlebar dropdown (`state.openMenu = -1`). Presses that call `stopPropagation()` never reach the workspace's own "close the menu" handler, so they call this themselves.

**`static void swallowPress(View &v)`**: Makes a press on `v` belong to it alone (it stops propagation). Used on titlebar widgets: without it the press would bubble to the titlebar's `onPointerDown`, which closes menus and calls `requestMove()` (a window drag that swallows the release, so the click never arrives).

**`static void registerTextField(const std::shared_ptr<TextInputState> &st, const View &flattened)`**: Registers a name field's key handler. Call right after `parent.addChild(std::move(textInput))`, with that input's shared state and the view it flattened into (`parent.children.back()`). The registry is only ever appended to.

**`static bool forwardKeyToTextField(const KeyEvent &e)`**: **Returns** true if a registered field has focus and took the key: it runs that field's own `onKeyDown`. Called by the global shortcut for the plain keys (Left, Right, Delete, Enter) so they reach a focused text field instead of being swallowed by the shortcut.

**`static void bindDrag(View &v, std::function<void(float, float)> press, std::function<void(float, float)> drag, std::function<void()> end, std::function<void(float, float)> rightPress = nullptr, Cursor hover = Cursor::Inherit, Cursor dragging = Cursor::Inherit)`**: A left-button drag gesture on one view, built on pointer events. **Inputs:** `press(x, y)`, `drag(x, y)` and `end()` callbacks (the points are view-local); optional `rightPress(x, y)`; optional `hover` and `dragging` cursors. The press captures the pointer, so `drag` and the release keep arriving even when the pointer leaves the view. The gesture belongs to **one pointer** (`owner`): touch can put several fingers on the same view and only the first drives the drag; the others are ignored until it ends (for the mouse the owner is always pointer 1). The press stops propagation and closes any open menu. `end()` runs on release **and** on pointer cancel (focus loss, the OS taking the pointer). With a `hover` cursor the view shows it, or `dragging` (default: the same) while a drag is in progress.

**`static void applyClickCursors(View &v)`**: Gives every view that has an `onClick` the hand cursor, unless it already chose one (the cursor style is inherited, so its label gets it too). Recurses over the tree; run once on the finished tree from `main()`.

**`static View menuButton(const std::string &label, int id, std::vector<MenuItem> items)`**: **Inputs:** the label, a menu id (the index in the title bar) and the `MenuItem`s. A titlebar button ("File", "View", ...) that opens a dropdown below it when clicked. Only one menu is open at a time (`state.openMenu`); picking an item runs its action and closes the menu.

**`static View winButton(const std::string &glyph, Color hoverBg, std::function<void()> onClick)`**: One of the three window-control buttons (minimize / maximize / close): a glyph, the background shown on hover and the click action.

**`static View titlebarIconBtn(const std::string &glyph, std::function<void()> onClick, std::function<bool()> enabled =`**: A small square titlebar icon button (undo / redo, ...). `enabled` is optional: pass it to dim the glyph when the action isn't currently available. The click is always safe to wire up as a no-op in that case (`undo()` / `redo()` check before doing anything).

**`static View sidebarButton(const std::string &label, std::function<void()> onClick, std::function<bool()> selected =`**: A full-width sidebar button. `selected` (optional) draws it highlighted: used for the aspect-ratio presets, which behave like radio buttons. `primary` gives it the accent fill for the pane's main action.

**`static View buildSliderRow(const SliderSpec &spec)`**: A single Lightroom-style slider row: a label with the live value, then a draggable track. Reads and writes go through the spec's `get` / `set` rather than binding a raw `float&`: that is what lets `AdjustmentSet` coalesce a whole drag into one undo step instead of one per frame.

**`static View buildAdjustmentSlider(const AdjustmentSpec &spec)`**: One Adjust-pane slider for one `AdjustmentSpec` (the engine's table `kAdjustmentSpecs`): adding a new adjustment to that table needs no new UI code at all.

### `color_ui.hpp`
The building blocks of the **Color Mix** and **Color Grade** panes. Colour Mix: one slider for a band's Hue / Saturation / Luminance (`buildMixSlider`) and the reset. Colour Grade: the three-way mode (Midtones large, Shadows and Highlights below) and the single-range modes (Shadows / Midtones / Highlights / Global), built from hue-saturation wheels (`buildGradeWheel`), eye toggles, luminance sliders and Blending / Balance. A wheel is a pre-rendered disc bitmap (`gradeWheelImage`) plus a vector puck: angle = hue (0 at 3 o'clock, counter-clockwise), radius = saturation. The panes themselves, which assemble these, are in `panes/mix_pane.hpp` and `panes/grade_pane.hpp`; the selected tab (`uiState.mix.chan`, `uiState.grade.mode`, `uiState.grade.active`) is in `ui_state.hpp`; the maths is the engine's `color_mix.hpp` / `color_grade.hpp`.

**Depends on:** `render_update.hpp`, `widgets.hpp`; engine: `adjustment_set.hpp`, `color_grade.hpp`, `color_mix.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kGradePaneW` | `float` | `250.0f` | sidebar width the wheels are laid out for |
| `kGradePi` | `float` | `3.14159265358979f` | π, as used by the wheel maths |
| `kGradeMainWheel` | `int` | `130` | Midtones wheel in 3-way mode |
| `kGradeSmallWheel` | `int` | `84` | Shadows / Highlights in 3-way mode |
| `kGradeSingleWheel` | `int` | `170` | wheel in the single-range modes |
| `kGradeTabW` | `float` | `40.0f` | Width of one mode tab (3-way, Shadows, Midtones, Highlights, Global) |

Wheel brightness is cosmetic (Lightroom dims the shadow wheel).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kGradeWheelBrightness[kGradeRangeCount]` | `float` | `{0.62f, 1.0f, 0.80f, 1.0f}` |  |

**Functions**

**`static View buildMixSlider(MixChan chan, size_t band)`**: **Inputs:** the mixer channel (Hue / Saturation / Luminance) and the band index. One Colour Mixer slider tinted in the band's own colour (`mixBandColor`). Same undo / preview behaviour as every Adjust slider (an edit coalesced into one undo step per drag).

**`static void resetColorMix()`**: Zeroes all 24 mixer sliders (8 bands × Hue, Saturation, Luminance) in one undo step.

**`static const char *gradeRangeName(int r)`**: **Input:** a range index 0..3. **Returns** "Shadows", "Midtones", "Highlights" or "Global". No bounds check.

**`static Color gradeHsv(float h, float s, float v, uint8_t a = 255)`**: **Inputs:** hue (degrees), saturation 0..1, value 0..1 and an optional alpha (default 255). **Returns** the colour, with hue from `gradeHueToRgb` and saturation as a blend towards white, then scaled by the value. Used for the puck and the wheel's tints.

**`static size_t gradeHash(float v)`**: Hash of a float (`std::hash<float>`), for the wheel-image cache key.

**`static size_t gradeMix(size_t a, size_t b)`**: Combines two hashes into one (the usual `a ^ (b + 0x9e3779b97f4a7c15 + (a << 6) + (a >> 2))` mix), for the wheel-image cache key.

**`static const CanvasImage &gradeWheelImage(int d, float brightness)`**: **Inputs:** the diameter in **device** pixels and a brightness. **Returns** a reference to the pre-rendered hue / saturation disc for that pair: angle = hue (0 at 3 o'clock, counter-clockwise), radius = saturation, white in the centre. Cached per (diameter, brightness) in a map so the image address stays stable (LiteUI caches the converted surface per image). Brightness is cosmetic: `kGradeWheelBrightness` dims the Shadows wheel as Lightroom does.

**`static void gradeEdit(const std::function<void(ColorGrade &)> &fn, int key)`**: **Inputs:** a function that edits a `ColorGrade`, and an edit key. Applies the function to a copy of the current grade and commits it through `adjust.setGrade`. Edits with the same non-negative key are coalesced into one undo step (see `AdjustmentSet::setGrade`); the wheel drag uses the range index, the sliders `keyBase + range`, and discrete actions such as the right-click reset use -1 for a step of their own.

**`static void gradeEndDrag()`**: Ends a wheel or slider drag: leaves the low-resolution drag preview and asks for one full-resolution render (`adjust.endGradeEdit()` closes the coalesced edit).

**`static View buildGradeWheelSlider(const std::string &name, int range, float GradeWheel::*field, float lo, float hi, int keyBase, float trackW, bool isSigned)`**: **Inputs:** the label, the wheel (range), a pointer-to-member selecting its field (hue, saturation or luminance), the range lo / hi, an edit key base, the track width and whether the value prints with a sign. A slider bound to one field of one wheel: writes the rounded value through `gradeEdit`, makes that wheel the active one (`uiState.grade.active`) and ends the edit on release.

**`static View buildGradeWheel(int range, int diameter, float pad)`**: **Inputs:** the range, the disc diameter and the padding. One hue / saturation wheel (a canvas): click or drag sets hue from the angle and saturation from the radius (rounded; the hue is kept while the pointer sits on the centre); the drag enters the preview path. **Right-click** resets hue and saturation of that wheel but keeps its luminance. It paints the disc (dimmed to 30% while the wheel's preview eye is off), a spoke from the centre to the puck, and the puck.

**`static View buildGradeEye(int range)`**: **Input:** the range. The eye icon that toggles that wheel's preview (`visible`), one undo step.

**`static View buildGradeWheelBlock(int range, int diameter, float pad)`**: **Inputs:** range, diameter, padding. The wheel, its eye row and its luminance slider as one column.

**`static View buildGradeThreeWay()`**: 3-way mode: the big Midtones wheel on top (`kGradeMainWheel`), Shadows and Highlights below it (`kGradeSmallWheel`).

**`static View buildGradeSingle(int range)`**: **Input:** the range. Single-range mode: one large wheel (`kGradeSingleWheel`) with Hue / Saturation / Luminance sliders.

**`static void resetColorGrade()`**: Resets every wheel, Blending and Balance in one undo step.

## Masks, presets, snapshots and virtual copies

### `masks.hpp`
The base of the **Masks pane** (activity 7), which edits `state.adjust.masks()`: the overlay colour, which mask and which part is selected, the in-view form of a shape, the one function every mask edit goes through (`maskEdit`) and the mask name field. Sliders go through `buildSliderRow` like everywhere else; the shape can also be dragged on the canvas (press inside = move, press a square handle = resize). Masks are *stored* against the original picture; the canvas works on the **in-view** form (`shapeInView`, `editShapeInView`) so that outlines, hit tests and drags match what is on screen after crop / rotate / straighten. **Undo keys:** the undo key of a mask edit is `(mask × kMaxMaskParts + part) × kMaskCtlStride + control`, so repeated edits of the same control of the same mask (and part) fold into one undo step. Every control id of one mask must be different and below `kMaskCtlStride`, or two controls would fold into one step (a `static_assert` in the file checks it). The rest of the Masks code is in `mask_list.hpp`, `mask_shapes.hpp`, `mask_polygon.hpp`, `mask_brush.hpp`, `mask_overlays.hpp`, `mask_widgets.hpp`, `mask_effects.hpp` and `mask_curve.hpp`; the pane is `panes/masks_pane.hpp`.

**Depends on:** `render_update.hpp`; engine: `adjustment_set.hpp`, `geometry.hpp`, `mask.hpp`, `mask_brush.hpp`, `mask_geom.hpp`, `mask_polygon.hpp`, `mask_rect.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

The Masks pane (activity 7) edits `state.adjust.masks()`. Sliders go through buildSliderRow like everywhere else; the shape can also be dragged on the canvas (press inside = move, press a square handle = resize). Undo key of a mask edit: (mask * kMaxMaskParts + part) * kMaskCtlStride + control. Every control id of one mask must be different and below kMaskCtlStride, or two controls would fold into one undo step (a static_assert below checks it).

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kMaskCtlStride` | `int` | `1000` | Multiplier in the undo key; every control id of one mask must be below it |
| `kMaskCtlMove` | `int` | `90` | undo-coalescing control ids (see maskEdit) |
| `kMaskCtlResizeX` | `int` | `91` | Undo control id of dragging the horizontal-axis handle |
| `kMaskCtlResizeY` | `int` | `92` | Undo control id of dragging the vertical-axis handle |
| `kMaskCtlLinEnd` | `int` | `93` | Undo control id of dragging a linear mask's end line |
| `kMaskCtlLinStart` | `int` | `94` | Undo control id of dragging a linear mask's start line |
| `kMaskCtlBrush` | `int` | `95` | one stroke = one undo step |
| `kMaskCtlLumLo` | `int` | `96` | Luminosity range: Min slider |
| `kMaskCtlLumHi` | `int` | `97` | Luminosity range: Max slider |
| `kMaskCtlPolyVertex` | `int` | `98` | a vertex placed / dragged = one undo step |
| `kMaskCtlPolyMove` | `int` | `99` | dragging the whole polygon |
| `kMaskCtlPolyLasso` | `int` | `87` | one freehand lasso = one undo step |
| `kMaskCtlRange` | `int` | `84` | Colour Range: Range slider |
| `kMaskCtlFeather` | `int` | `1` | Shape slider: Feather |
| `kMaskCtlAngle` | `int` | `2` | Shape slider: Angle |
| `kMaskCtlWidth` | `int` | `3` | Shape slider: Width |
| `kMaskCtlHeight` | `int` | `4` | Shape slider: Height |
| `kMaskCtlLength` | `int` | `5` | Shape slider: Length |
| `kMaskCtlAdj` | `int` | `100` | + AdjId (100..100+Count-1, clear of every id below) |
| `kMaskCtlName` | `int` | `86` | The mask name field |

**Types**

- `struct MaskTintRgb`: One overlay colour: red, green, blue and a name.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `r` | `uint8_t` |  |  |
  | `g` | `uint8_t` |  |  |
  | `b` | `uint8_t` |  |  |
  | `name` | `const char *` |  |  |


**State and tables**

- `kMaskTintPalette[]`: `constexpr MaskTintRgb`, initially `{{255, 40, 40, "Red"}, {60, 220, 90, "Green"}, {70, 140, 255, "Blue"}…`
- `kMaskTintCount`: `constexpr int`, initially `static_cast<int>(sizeof(kMaskTintPalette) / sizeof(kMaskTintPalette[0…`. Number of palette entries.

**Functions**

**`static const MaskTintRgb &maskTint()`**: **Returns** the palette entry chosen by `state.maskTintColor`, clamped to the palette.

**`static Color maskTintColor(uint8_t alpha)`**: **Input:** an alpha. **Returns** the overlay colour with that alpha.

**`static void setMaskTintColor(int idx)`**: **Input:** a palette index (clamped). Picks the overlay colour and drops the cached tint bitmaps (brush, luminosity, combined), which carry the old colour; they are rebuilt on the next paint. Session-only: the choice is not saved.

**`static bool maskToolActive()`**: True while the Masks pane (activity 7) is open and a picture is loaded: the canvas then draws the mask overlay and handles its pointer events.

**`static float maskChipW(int n)`**: **Input:** n. **Returns** the width of one of n equal chips / tabs sharing a row of the mask panel: `(panel width − padding − scrollbar − 4 × (n−1)) / n`, at least 20.

**`static int selMask()`**: **Returns** the selected mask's index, or -1. Clamps `state.maskSel` into `[-1, count-1]` here, so no place that can shrink the list (undo, redo, open, delete) needs to fix it up.

**`static void selectMask(int i, bool force = false)`**: **Inputs:** a mask index (-1 = none) and `force`. Selects it. The Curve tab's selected point belongs to the mask it was picked on, so it is dropped when another mask is selected. `force` is for when the list changed under the selection so the same index may now be a different mask (delete); a plain click on the mask that is already selected keeps its point.

**`static const Mask *selectedMask()`**: **Returns** a pointer to the selected mask in `adjust.masks()`, or null.

**`static int selPart()`**: **Returns** the index of the selected mask's part that the canvas and the shape sliders work on (a mask can hold several parts; see `MaskComposite`). Clamped on read like `selMask()`, so undo / delete / open never need to fix it up. 0 with no mask.

**`static const MaskPart *selectedPart()`**: **Returns** the selected part of the selected mask, or null.

**`static MaskType selectedPartType()`**: **Returns** the `MaskType` of the selected part, or `MaskType::Count` when there is none.

**`static bool selectedIsBrush()`**: True when the SELECTED PART is a brush.

**`static bool selectedIsPolygon()`**: True when the SELECTED PART is a polygon / lasso.

**`static bool &tintFlag()`**: **Returns** a reference to the "Show mask tint" switch for the selected mask: a mask that is a single brush keeps its own switch (`mask.brushTint`, on by default) because it has no outline; everything else shares `state.maskShowTint`.

**`template <class Shape> static std::optional<Shape> shapeInView(const MaskPart &p, const GeoFrame &f)`**: **Inputs:** a part and the `GeoFrame` of the picture being shown. **Returns** the part's shape of type `Shape` converted to the picture as it is shown (`toSource(f)`), or `std::nullopt` if the part is a different shape. Everything on the canvas (outlines, hit tests, drags) works on this form.

**`static std::optional<RadialShape> radialInView(const MaskPart &p, const GeoFrame &f)`**: `shapeInView<RadialShape>`.

**`static std::optional<LinearShape> linearInView(const MaskPart &p, const GeoFrame &f)`**: `shapeInView<LinearShape>`.

**`static std::optional<BrushShape> brushInView(const MaskPart &p, const GeoFrame &f)`**: `shapeInView<BrushShape>`.

**`static std::optional<RectShape> rectInView(const MaskPart &p, const GeoFrame &f)`**: `shapeInView<RectShape>`.

**`static std::optional<PolygonShape> polygonInView(const MaskPart &p, const GeoFrame &f)`**: `shapeInView<PolygonShape>`.

**`template <class Shape> static void editShapeInView(Mask &m, size_t part, const GeoFrame &f, const std::function<void(Shape &)> &fn)`**: **Inputs:** a mask, a part index, the frame and an edit function. Edits part `part` on its in-view form and stores the result back, changing only the fields the function changed (`applySourceEdit`). Does nothing for a bad part index or a part of another shape type.

**`static void editRadialInView(Mask &m, size_t part, const GeoFrame &f, const std::function<void(RadialShape &)> &fn)`**: `editShapeInView<RadialShape>`.

**`static void editLinearInView(Mask &m, size_t part, const GeoFrame &f, const std::function<void(LinearShape &)> &fn)`**: `editShapeInView<LinearShape>`.

**`static void maskEdit(int idx, const std::function<void(Mask &)> &fn, int control)`**: **Inputs:** a mask index, an edit function and a control id. Edits mask `idx` in a copy of the list and commits it with `adjust.setMasks`. `control >= 0` names the control being dragged: repeated edits of the same control of the same mask (and, for the selected mask, the same part) fold into one undo step; the undo key is `(idx × kMaxMaskParts + part) × kMaskCtlStride + control`. `-1` = a discrete change with its own step. It does **not** recompute the picture: slider and canvas drags do that through the usual `markAdjustDirty` path; discrete actions call `maskEditDiscrete`.

**`static void maskEditDiscrete(const std::function<void(Mask &)> &fn)`**: A one-off change to the selected mask (toggle, reset): its own undo step and an immediate recompute (`updateImageFromAdjustments`). No-op with no mask selected.

**`static bool typingInTextField()`**: True while a name field (mask, preset or snapshot) has keyboard focus. The global shortcuts that would also fire on plain keys (Y, J, Left, Right, Delete, ...) must stand down, or typing a name would toggle views and Delete would delete the mask.

**`static void syncMaskNameField()`**: Makes the mask name field show the selected mask's name (text, cursor at the end, scroll reset). Cheap; polled from the field's display lambda, so it also catches undo / redo and selection changes. No-op if the field doesn't exist yet or already shows it.

**`static void renameSelectedMask(const std::string &typed)`**: **Input:** the typed text. Sanitizes it (`sanitizeMaskName`: the cap or a stripped character may change what was typed, and then the field is rewritten) and commits it with `maskEdit(..., kMaskCtlName)`, so typing folds into one undo step.

### `presets.hpp`
The **Presets pane's** logic and **Copy / Paste Settings**. A preset is a saved look (sliders, curves, Colour Mixer, Colour Grading; see the engine's `io/preset.hpp`). Presets live as small files in `presetDir()`; the list (`uiState.presets.list`) is a cache of what is there, re-read when the pane is opened and after every save or delete (never per frame). Applying a preset is one undo step and does not touch crop, rotation or masks. Copy / Paste uses the same payload (the look only), kept in memory in `uiState.clipboard.settings` instead of a file. The pane itself is `panes/presets_pane.hpp`.

**Depends on:** `saved_edits.hpp`; engine: `adjustment_set.hpp`, `file_utils.hpp`, `preset.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void refreshPresets(const std::string &select = std::string())`**: **Input:** an optional preset name to select. Re-reads the presets folder into `uiState.presets.list`, at most `kPresetRows` entries, skipping damaged files (not fatal). The selection is reset to -1, or to the entry with the given name.

**`static void applyPresetEntry(size_t i)`**: **Input:** a list index. Replaces the look with preset i: one undo step, "Preset: <name>" in History. Crop, rotation and masks are not touched. Ignored while dragging, with no picture or for a bad index. Remembers the row as the selection. If the picture already has that look it says "Already looks like \"<name>\""; otherwise drops the curve editor's selected point (the preset replaced the curves), recomputes and says "Applied preset: <name>".

**`static void saveCurrentPreset()`**: Saves the current look under the name typed in the field (`sanitizePresetName`). With an empty name: "Type a name for the preset first". On success re-reads the list selecting the new one, clears the field and says "Saved preset: <name>"; on failure "Couldn't save preset: <reason>".

**`static void deleteSelectedPreset()`**: Deletes the selected preset's file, re-reads the list and says "Deleted preset: <name>" (or "Couldn't delete <file>"). No-op with no valid selection.

**`static void filterPresetNameField(const std::string &typed)`**: **Input:** the typed text. Keeps the name field to printable ASCII (the `TextInput` is ASCII-only) and to `kPresetNameMax` characters, rewriting the field if it changed. Trimming happens on save, so typing spaces between words still works.

**`static void copySettings()`**: Copies the open picture's look (the same payload as a preset) into `uiState.clipboard.settings`, remembering the picture's label in `settingsFrom`, and says "Copied settings from <picture>". Ignored with no picture.

**`static void pasteSettings()`**: Pastes the copied look onto the open picture: one undo step. Ignored while dragging or with no picture; "Copy settings from a photo first" when nothing was copied. "Already has these settings" if nothing would change; otherwise drops the curve editor's selected point, recomputes and says "Pasted settings from <picture>".

### `snapshots.hpp`
The **Snapshots pane's** logic. A snapshot is a named, complete copy of the picture's edits: sliders, curves, Colour Mixer, Colour Grading, crop, rotation, straighten **and masks** (`NamedSnapshot` in `io/sidecar.hpp`). They live in `state.snapshots`, are saved in the picture's sidecar with everything else (`snapshotsDirty` makes autosave write when only the list changed), and are not edits themselves: adding or deleting one is not an undo step. Restoring one **is**: one step, "Snapshot: <name>". The pane is `panes/snapshots_pane.hpp`.

**Depends on:** `render_update.hpp`; engine: `adjustment_set.hpp`, `sidecar.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static std::string nextSnapshotName()`**: **Returns** "Snapshot 1", "Snapshot 2", ...: the first one no snapshot already uses (plain "Snapshot" if every number up to `kMaxSnapshots + 1` is taken).

**`static void filterSnapshotNameField(const std::string &typed)`**: **Input:** the typed text. Keeps the snapshot name field to printable ASCII and to the name cap. Trimming happens on save, so typing spaces between words still works.

**`static void addSnapshot()`**: Saves the picture's current edits (`adjust.currentEdits()`) as a snapshot named like the field (`sanitizeSnapshotName`) or `nextSnapshotName()` when it is blank. Ignored with no picture. Refuses with a message when the picture already holds `kMaxSnapshots` ("delete one first") or when the sidecar's snapshot JSON would exceed `kSnapshotsMaxBytes` (every snapshot repeats every mask, object bitmaps included, so the list is bounded in bytes as well as in count: "Too many masks to keep another snapshot"). Not an undo step; marks `snapshotsDirty` so the sidecar is written.

**`static void applySnapshotEntry(size_t i)`**: **Input:** a snapshot index. Puts the picture back as snapshot i describes it: **one undo step**. Geometry and masks come back too, so the picture is refreshed exactly as after an undo (and both curve editors' selected points are dropped).

**`static void deleteSelectedSnapshot()`**: Deletes the selected snapshot (`uiState.snapshots.sel`), clears the selection, marks `snapshotsDirty` and says "Deleted snapshot: <name>". Not an undo step. No-op with no valid selection.

### `virtual_copies.hpp`
**Virtual copies**, then the **Object mask** commands and adding / deleting masks (the file keeps the order the code had in `main.cpp`, so it holds two topics). A virtual copy is another set of edits for the same picture file: its own sidecar (`photo.jpg.vc2.literoom.json`), its own filmstrip cell right after the picture's, its own rating, flag, label and snapshots (see the engine's `virtual_copy.hpp`). Opening one of the open picture's copies reuses the pixels (see `filmOpenIndex`), so switching is instant. The object-mask part is the UI side of the segmenter in `segmentation.hpp`: clicks become prompts, a request goes to `segmentWorker`, and `adoptSegment` stores the result.

**Depends on:** `render_update.hpp`, `segmentation.hpp`, `saved_edits.hpp`, `sync.hpp`, `filmstrip.hpp`, `masks.hpp`; engine: `adjustment_set.hpp`, `file_utils.hpp`, `geometry.hpp`, `mask.hpp`, `mask_brush.hpp`, `mask_geom.hpp`, `mask_object.hpp`, `mask_polygon.hpp`, `mask_range.hpp`, `mask_rect.hpp`, `photo_meta.hpp`, `sidecar.hpp`, `thumb_worker.hpp`, `virtual_copy.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static void objectPrefetch()`

**Functions**

**`static void filmInsertEntry(size_t at, FilmEntry e)`**: **Inputs:** an index and a `FilmEntry`. Puts the entry at `at` in the strip and keeps every stored index right: `current`, `target` and `anchor` shift up when they were at or after `at`, the name index is rebuilt and the view is re-filtered.

**`static void filmRemoveEntry(size_t at)`**: **Input:** an index. Takes the entry out of the strip. `current` / `target` / `anchor` that pointed at it become -1 (the caller then opens something else); later ones shift down. Rebuilds the name index and the view.

**`static void createVirtualCopy()`**: A new virtual copy of the open picture, starting from its current edits (snapshots and the rating are not copied), opened at once. Ignored without a picture, while dragging or while a sync runs. Finds the picture's last cell (itself and its copies) in the strip, and the first free copy number (`vcFreeId`); at `kMaxVirtualCopies` it says so. Flushes the open picture's sidecar, writes the copy's sidecar (`sidecarFileText` of the current edits, no meta) atomically, inserts a cell right after the last one (with a thumbnail made from the current image), opens it and says "Created virtual copy N".

**`static void deleteVirtualCopy()`**: Deletes the open virtual copy's edits file (the picture and its other copies are untouched) and goes back to the original. **Asks to be chosen twice:** the first time it only remembers the picture (`uiState.vcDeleteArmed`) and says "Choose Delete Virtual Copy again to delete copy N and its edits". Refuses on the original ("Only a virtual copy can be deleted"). After removing the file it marks everything saved, so nothing is written back when the editor moves on, removes the strip cell and opens the original.

**`static void setSelectedObjectMap(std::shared_ptr<const ObjectMap> m)`**: **Input:** an `ObjectMap`. Sets it as the map of the selected object part (one undo step, `maskEditDiscrete`) and turns the mask tint on. Used by the test-map button.

**`static std::shared_ptr<const ObjectMap> makeTestObjectMap()`**: **Returns** the "Set test map" button's selection (not the segmenter): a map at most `kObjectMapEdge` on the long side where a pixel is on (255) if its luminance (54 R + 183 G + 19 B) is above the picture's mean, else off. Null without a picture.

**`static bool selectedIsObject()`**: True when the selected part is an Object part.

**`static std::vector<ObjectPrompt> objectCurrentPrompts(int mask, int part)`**: **Inputs:** a mask and part index. **Returns** the prompts a new click builds on: those still being segmented for this part (`state.mask.objPending`) if it is for the same mask and part, else the committed prompts of that part. Original-picture fractions. Empty for a bad index or a non-object part.

**`static void clearSelectedObject()`**: Clears the selected object part: invalidates the segmenter, drops pending prompts, and (one undo step) removes the part's map and prompts.

**`static void objectPrefetch()`**: Warms the segmentation model for the open picture: submits a `prefetch` request (encode only) unless there is no picture, a request is already running, or this picture (`imageSerial`) is already encoded. Says "Preparing object selection...".

**`static void objectPress(float lx, float ly, bool rightClick)`**: A click on the picture with an object part selected. **Inputs:** the canvas-local position and whether it was a right click. Left click adds a point (foreground, or background if "Click adds Background" is on); right click adds the opposite kind; **clicking an existing dot removes it** (within `kMaskHitPx / zoom`). A click is converted view → original through the `GeoFrame` and ignored if it falls outside the picture (straighten's empty corners); prompts are stored as fractions of the original. If the last dot was removed the selection is cleared. Otherwise a `SegmentRequest` with all the prompts is submitted and the prompts are shown as pending. Status: "Selecting object..." or "Analyzing image (first selection takes a moment)..." when the model hasn't encoded this picture yet.

**`static std::optional<MaskShape> newMaskShape(MaskType t)`**: **Input:** a `MaskType`. **Returns** the starting shape of that kind: radial, linear and rectangle centred on the picture as shown now (`fromSource` with the current `GeoFrame`); an empty brush (the first drag paints); a luminosity range (not spatial); an empty object; a colour range (nothing selected until a colour is clicked); an empty polygon (clicks place the vertices). `std::nullopt` for an unknown type.

**`static void addMask(MaskType t)`**: **Input:** the type. Adds a new mask with one `Add` part of that shape: one undo step, selected at once, part 0. Refuses at `kMaxMasks` ("Mask limit reached (N)"). A new mask has no sliders moved, so no pixels change (overlay repaint only). For an Object mask it also starts `objectPrefetch()`.

**`static void deleteSelectedMask()`**: Removes the selected mask (one undo step), selects the one that slid into its index (or the last, with `force` so the curve selection is dropped), resets the part to 0 and recomputes the picture.

### `mask_list.hpp`
Operations on the mask list and on a mask's parts: duplicate, reorder, Copy / Paste Effects, add / change / remove a part, and `maskEndDrag`. Masks apply in list order (later ones on top, and a luminosity range sees the result of the masks before it), so order matters. A mask can hold several **parts** combined with `Add`, `Subtract` and so on (`MaskComposite`); the first part has no operation. Every function here is one undo step and recomputes the picture, through `maskEditDiscrete` or `setMasks`.

**Depends on:** `render_update.hpp`, `masks.hpp`, `virtual_copies.hpp`; engine: `adjustment_set.hpp`, `mask.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void duplicateSelectedMask()`**: Inserts a copy of the selected mask (shapes and effects) right after it and selects the copy; a non-blank name gets " copy" appended. One undo step. Refuses at `kMaxMasks` ("Mask limit reached (N)"). No-op with no selection.

**`static void moveSelectedMask(int dir)`**: **Input:** -1 (up) or +1 (down). Swaps the selected mask with its neighbour, keeps it selected, one undo step, recomputes. No-op at the ends or with no selection.

**`static void copyMaskEffects()`**: Copies the selected mask's effects (Amount, Adjust values, Colour Mix, Colour Grading, curves; **not** its shape) into `uiState.clipboard.maskEffects` and says "Mask effects copied".

**`static void pasteMaskEffects()`**: Pastes them onto the selected mask (one undo step, immediate recompute). "Copy a mask's effects first" when nothing was copied. The shape is untouched.

**`static void addPart(MaskType t)`**: **Input:** a `MaskType`. Adds a part of that kind to the selected mask (joined with `Add`; change it with `setPartOp`) and selects it. The combined result is what matters from here on, so the tint is switched on.

**`static void setPartOp(MaskOp op)`**: **Input:** a `MaskOp`. Sets how the selected part joins the ones before it. The first part has none, so it is ignored there.

**`static void removeSelectedPart()`**: Removes the selected part (a mask always keeps at least one), then selects the one before it (`selPart` clamps).

**`static void maskEndDrag()`**: A drag on the canvas or a mask slider ended: clears the drag, ends the coalesced mask edit (`adjust.endMaskEdit()`), leaves the low-resolution preview and asks for one full-resolution render (the same job as `gradeEndDrag`).

### `mask_shapes.hpp`
Geometry of the radial, linear and rectangle masks **on the canvas**, in document pixels (the picture as it is shown). Converts a shape's stored fractions into pixels (`MaskGeom`, `LinGeom`) and answers "what is under this point?" for the body and for the drag handles. Handles are only hit-tested for the selected mask (`handles = true`) and win over the body. The tolerance is `kMaskHitPx / zoom`, so handles feel the same size at every zoom.

**Depends on:** engine: `mask_geom.hpp`, `mask_rect.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `struct MaskGeom`: A radial or rectangle mask in document pixels: centre, half-axes, the angle in radians (clockwise) with its cosine and sine, and `S`, the shorter document side (the unit radii are measured in).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `cx` | `float` |  | center |
  | `cy` | `float` |  |  |
  | `rx` | `float` |  | half-axes |
  | `ry` | `float` |  |  |
  | `ang` | `float` |  | radians, clockwise |
  | `c` | `float` |  | cos, sin of ang |
  | `s` | `float` |  |  |
  | `S` | `float` |  | the shorter document side (radius unit) |

- `enum kMaskHitNone ...`: The result of a hit test: nothing, the body, the horizontal-axis handle (`X`), the vertical-axis handle (`Y`), the linear mask's end line (`LinEnd`) or its start line (`LinStart`). Values: `kMaskHitNone`, `kMaskHitBody`, `kMaskHitX`, `kMaskHitY`, `kMaskHitLinEnd`, `kMaskHitLinStart`.
- `struct LinGeom`: A linear mask in document pixels: centre, unit direction, half-length, angle and `S`. `sx()` / `sy()` give the start point (centre minus direction × length).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `cx` | `float` |  | center |
  | `cy` | `float` |  |  |
  | `ux` | `float` |  | unit direction of fade-out |
  | `uy` | `float` |  |  |
  | `len` | `float` |  | half length of the ramp |
  | `ang` | `float` |  | radians |
  | `S` | `float` |  | the shorter document side |

  - `float sx() const`: The weight-1 line's and the weight-0 line's axis points.
  - `float sy() const`: y of the start line's axis point.
  - `float ex() const`: x of the end line's axis point (`cx + ux × len`).
  - `float ey() const`: y of the end line's axis point.

**Functions**

**`static MaskGeom radialGeom(const RadialShape &r)`**: **Input:** a `RadialShape` (fractions). **Returns** its geometry in document pixels: centre `(cx × W, cy × H)`, half-axes `rx` and `ry` in units of the shorter side `S`, the angle in radians (clockwise) and its cosine / sine.

**`static int maskHit(const RadialShape &r, float px, float py, bool handles)`**: **Inputs:** a radial shape, a point in document pixels, and whether handles count. **Returns** `kMaskHitX` or `kMaskHitY` (the four handles on the rotated axes at the ellipse outline), `kMaskHitBody` (inside the ellipse) or `kMaskHitNone`. Handles win over the body.

**`static LinGeom linearGeom(const LinearShape &l)`**: **Input:** a `LinearShape`. **Returns** its geometry: centre, unit direction `(ux, uy)` from the angle, the half-length `len` (at least `kMaskMinRadius`) × `S`, and `S`.

**`static int linearHit(const LinearShape &l, float px, float py, bool handles)`**: **Inputs:** as `maskHit`. **Returns** `kMaskHitLinEnd` / `kMaskHitLinStart` (handles where the axis crosses the two lines; selected mask only), `kMaskHitBody` (the band between the lines) or `kMaskHitNone`.

**`static MaskGeom rectGeom(const RectShape &r)`**: **Input:** a `RectShape`. Same layout as the radial mask: a centre, half-sizes along the rotated axes and four handles on the axes (here at the middle of each side).

**`static int rectHit(const RectShape &r, float px, float py, bool handles)`**: **Inputs:** as `maskHit`. **Returns** `kMaskHitX` / `kMaskHitY` for the side handles (selected part only), `kMaskHitBody` inside the rectangle, or `kMaskHitNone`.

### `mask_polygon.hpp`
The **Polygon / lasso** mask tool on the canvas. With a polygon part selected every press edits its outline (like a brush; pick another part from the list): click on empty space adds a vertex (on a closed outline: into the nearest side), and dragging before release places it; press on a vertex drags it, right-click on it removes it; press on a side inserts a vertex there and drags it; press inside moves the whole polygon. With **Freehand lasso** on, a press starts a new outline and the drag draws it, simplified while you draw. The outline is always closed once it has three points. Coordinates: vertices are stored as fractions of the picture (allowed from `kPolyClampLo` to `kPolyClampHi`, so a vertex may sit a little outside it) and measured on the canvas in document pixels. Each edit is one undo step per control (`kMaskCtlPolyVertex`, `kMaskCtlPolyMove`, `kMaskCtlPolyLasso`), closed by `maskEndDrag`.

**Depends on:** `render_update.hpp`, `masks.hpp`; engine: `geometry.hpp`, `mask_polygon.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

With a polygon part selected every press edits its outline (like a brush, pick another part from the list): click empty space adds a vertex (on a closed outline: into the nearest side), and dragging before release places it press on a vertex drags it; right-click on it removes it press on a side inserts a vertex there and drags it press inside moves the whole polygon With "Freehand lasso" on, a press starts a new outline and the drag draws it (simplified while you draw). The outline is always closed.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kPolyClampLo` | `float` | `-0.25f` | vertices stay near the picture |
| `kPolyClampHi` | `float` | `1.25f` | Highest allowed fraction |
| `kLassoTolPx` | `float` | `1.5f` | simplification tolerance, screen px |

**Functions**

**`static float polyClampCoord(float v)`**: Clamps a fraction to `[kPolyClampLo, kPolyClampHi]`.

**`static std::vector<PolyPt> polyDocPts(const PolygonShape &view)`**: **Input:** the polygon in view form (fractions). **Returns** its vertices in document pixels (what the pointer is measured in).

**`static int polyVertexAt(const std::vector<PolyPt> &dp, float px, float py)`**: **Inputs:** the vertices in document pixels and a point. **Returns** the index of the closest vertex within the grab tolerance `kMaskHitPx / zoom`, or -1.

**`static bool polygonBodyHit(const PolygonShape &view, float px, float py)`**: What picks an **unselected** polygon: a vertex, or a point inside a closed outline.

**`static void polygonEditPts(int mask, size_t part, int control, const std::function<void(std::vector<PolyPt> &)> &fn)`**: **Inputs:** mask index, part, an undo control id and an edit function over the vertex list. Edits the part's vertices in view form through `maskEdit` (so it folds into one undo step per control).

**`static void polygonDirty()`**: After a polygon change during a press / drag: enters the preview path, asks for the deferred recompute and repaints.

**`static void polygonBegin(float px, float py)`**: A press at document pixel `(px, py)` with a polygon part selected. In lasso mode: clears the raw path, starts the outline with this single point (control `kMaskCtlPolyLasso`) and begins a `Lasso` drag. Otherwise, in this order: on a **vertex** → begins a `PolyVertex` drag of it; on a **side** of a closed outline (within tolerance) → inserts a vertex there and drags it; **inside** a closed outline → begins a `PolyMove` drag of the whole polygon; **outside** a closed outline → inserts a vertex into the nearest side (the outline grows towards the pointer); with fewer than 3 points → appends a vertex. Refuses at `kPolyMaxPoints` ("Polygon point limit reached (N)").

**`static void polygonDragTo(float px, float py, MaskUi::Drag d)`**: Continues a polygon drag to `(px, py)`. `PolyVertex`: moves the dragged vertex (clamped). `PolyMove`: shifts every vertex by the pointer's movement, clamped so the whole outline stays within `kPolyClampLo..kPolyClampHi`; the remembered last point advances by what was really applied, so a clamped edge builds up no debt. `Lasso`: appends the point to the raw path if it moved at least `kLassoTolPx / zoom`, simplifies the path (`polySimplify`, doubling the tolerance until it fits `kPolyMaxPoints`) and stores the result as the vertices. Then `polygonDirty()`.

**`static void polygonRightPress(float lx, float ly)`**: Right-click on a vertex of the selected polygon removes it (one undo step). Ignored unless the mask tool is active, the selected part is a polygon and a vertex is under the pointer.

### `mask_brush.hpp`
The Masks pane's **pointer entry points** (`maskPress`, `maskDrag`) and the **brush** and **colour range** tools, plus the painting of the radial, linear, rectangle and polygon overlays. `maskPress` is the one function every press on the canvas goes through while the mask tool is active; it decides what the press means from the selected part's shape and what is under the pointer. With a brush part selected every press paints (pick another part from the list, or its pin if it is a brush): a stroke is recorded in document pixels of the picture on screen and stored through the `GeoFrame`, and the whole stroke is one undo step (control `kMaskCtlBrush`, closed by `maskEndDrag`). A brush mask has no body to grab; its **pin** (where its first stroke began) is what an unselected brush is picked by. Overlay painting for brush, luminosity and combined masks is in `mask_overlays.hpp`.

**Depends on:** `render_update.hpp`, `masks.hpp`, `virtual_copies.hpp`, `mask_shapes.hpp`, `mask_polygon.hpp`, `mask_overlays.hpp` (defined later); engine: `adjustment_set.hpp`, `geometry.hpp`, `mask.hpp`, `mask_brush.hpp`, `mask_geom.hpp`, `mask_object.hpp`, `mask_polygon.hpp`, `mask_range.hpp`, `mask_rect.hpp`, `render_core.hpp`, `render_params.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static std::shared_ptr<CanvasImage> lumTintBase()`: defined with the luminosity overlay below

**Functions**

**`static bool brushPinHit(const BrushShape &b, float px, float py)`**: **Inputs:** a brush in view form, a point in document pixels. True if the point is within `kMaskHitPx / zoom` of the brush's pin, the first point of its first stroke. False for a brush with no stroke.

**`static void brushBegin(float px, float py)`**: A press that starts a stroke at document pixel `(px, py)`. Builds a `BrushStroke` from the Brush tool's current settings (`brushErase`, `brushSize` converted to the stored form, `brushFeather`, `brushDensity`, `brushFlow`) with its first point stored through the `GeoFrame`, and appends it to the selected brush part with `maskEdit(..., kMaskCtlBrush)`. Refuses at `kBrushMaxStrokes` ("Brush stroke limit reached (N)"). Otherwise begins a `Paint` drag and remembers the point.

**`static void brushDragTo(float px, float py)`**: Extends the current stroke to `(px, py)`. A point is only recorded when the pointer has moved at least `max(radius × kBrushPointGap, 0.5 / zoom)` from the last one and the stroke has fewer than `kBrushMaxPoints`; each point goes through `maskEdit(..., kMaskCtlBrush)`, so the whole stroke stays one undo step. Enters the preview path and repaints; the cursor position is always updated for the cursor ring.

**`static std::optional<std::array<float, 3>> samplePreMaskColor(int sel, float fx, float fy)`**: **Inputs:** the mask index and a point as fractions of the picture on screen. **Returns** the colour there as it stands just **before** mask `sel` (the same pixels `ColorRangeEval` sees): renders the picture through the masks before `sel`, without Vignette / Grain (they run after the masks), and averages a 5×5 patch so one noisy pixel doesn't pick the colour. Components are 0..1. `std::nullopt` with no luminosity base image or a bad index.

**`static void colorRangePress(float px, float py)`**: A press with a Colour Range part selected: samples the colour under the pointer (`samplePreMaskColor`) and stores it in the part (`set = true`, `r`, `g`, `b`) as one undo step. Every press samples a new colour (like a brush, no handles).

**`static void maskPress(float lx, float ly)`**
**Inputs:** canvas-local press position. Does nothing unless the mask tool is active. Decides, in this order:
- **Brush part selected:** a press on another brush's pin selects that brush (last mask and part first); otherwise `brushBegin`.
- **Object part selected:** `objectPress` (every press is a prompt).
- **Polygon part selected:** `polygonBegin`.
- **Colour Range part selected:** `colorRangePress`.
- Otherwise it hit-tests shapes: first the **selected part** (with handles), then the selected mask's **other parts** (last first), then the **other masks** topmost first. On a hit it selects that mask and part and starts a drag: `ResizeX` / `ResizeY` for a side handle, `LinEnd` / `LinStart` for a linear line, `Move` for the body (remembering the grab offset). An unselected brush or polygon is only picked, not edited.

**`static void maskDrag(float lx, float ly)`**: **Inputs:** the canvas-local pointer position during a drag. `Paint` → `brushDragTo`; `PolyVertex` / `PolyMove` / `Lasso` → `polygonDragTo`. For a radial, rectangle or linear shape it edits the selected part in view form through `maskEdit` with the control id of the drag: `Move` sets the centre (pointer minus grab offset, clamped 0..1); `ResizeX` / `ResizeY` set a half-axis from the pointer's distance along the rotated axis (clamped to `kMaskMinRadius..kMaskMaxRadius`); `LinEnd` / `LinStart` set the half-length and also turn the axis so the handle stays under the pointer (the start handle's pull is negated). Then enters the preview path, asks for the deferred recompute and repaints.

**`static void paintRadialTint(CanvasContext &ctx, const RadialShape &r, bool invert)`**: Paints the red (overlay-coloured) tint showing a radial mask's weight (1 = solid, 0 = clear) over the picture; `invert` shows the inverse.

**`static void paintRadialOutline(CanvasContext &ctx, const RadialShape &r, bool selected)`**: Paints the outline of one radial mask; the selected one also gets its feather ring and its four handles.

**`static void paintRectOutline(CanvasContext &ctx, const RectShape &r, bool selected)`**: Paints the outline of one rectangle mask; the selected one also gets its feather rectangle, handles (middle of each side) and a ring at the centre.

**`static void paintPolygonOutline(CanvasContext &ctx, const PolygonShape &view, bool selected)`**: Paints the outline of one polygon mask. The outline is closed once it has three points; the selected one also shows a square handle on every vertex (the first one filled, so the start of a lasso is easy to see).

**`static void paintLinearTint(CanvasContext &ctx, const LinearShape &l, bool invert)`**: Paints the tint of a linear mask: a gradient along the axis from the weight-1 line to the weight-0 line, with stops sampled from the same smoothstep the renderer uses (the gradient itself is piecewise linear).

**`static void paintLinearOutline(CanvasContext &ctx, const LinearShape &l, bool selected)`**: Paints the outline of one linear mask: the two lines of the ramp; the selected one also gets the centre line, the axis with an arrowhead and its handles.

### `mask_overlays.hpp`
Painting of the mask **overlays** that need bitmaps or that tie the others together: the brush tint, pin and cursor ring, the luminosity / colour-range / combined tint bitmaps, the object prompt dots, the outline of any part, and `paintMaskOverlay`, which the canvas calls to draw everything for the Masks pane. The tint bitmaps are built from the **same** engine functions the renderer uses (`BrushShape::prepare`, `LuminosityShape::prepare`, `MaskComposite`), run on a small copy of the picture, so what is tinted is exactly what gets adjusted. They are cached (in `state.mask.tint`, `lumTint`, `compTint`) and rebuilt when the shape, the picture's pixels or the mask's place in the list change. The gradient and outline painters of radial, linear, rectangle and polygon masks are in `mask_brush.hpp`.

**Depends on:** `masks.hpp`, `mask_brush.hpp`; engine: `adjustment_set.hpp`, `geometry.hpp`, `mask.hpp`, `mask_brush.hpp`, `mask_object.hpp`, `mask_range.hpp`, `render_core.hpp`, `render_params.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static std::shared_ptr<CanvasImage> brushTintImage(const BrushShape &b, bool invert)`**: **Inputs:** a brush in view form and `invert`. **Returns** the red coverage bitmap: the brush rasterized at a small size through the same `BrushShape::prepare` the renderer uses. Cached until the strokes or the picture's size change (the shape is compared, which is cheap next to rasterizing it).

**`static void paintBrushTint(CanvasContext &ctx, const BrushShape &b, bool invert)`**: Draws the brush's tint over the whole picture. Nothing for a brush with no strokes unless `invert`.

**`static void paintBrushPin(CanvasContext &ctx, const BrushShape &b, bool selected)`**: Draws the pin that marks where a brush mask's first stroke began (all a brush has for an outline); the selected one solid.

**`static void paintBrushCursor(CanvasContext &ctx)`**: Draws the brush's footprint at the pointer while painting: the rim, and where the feather begins.

**`static bool sameOwner(const std::weak_ptr<CanvasImage> &a, const std::shared_ptr<CanvasImage> &b)`**: **Inputs:** a `weak_ptr` and a `shared_ptr` to a `CanvasImage`. True if both own the same object (compares owners without locking), used to tell whether a cached bitmap was built for the current picture.

**`static std::shared_ptr<CanvasImage> lumTintBase()`**: The small picture the tints are computed on: `state.source` box-downsampled by an integer factor to at most `kLumTintEdge` on its long side (the source itself when already that small). Cached.

**`static std::shared_ptr<CanvasImage> luminosityTintImage(const LuminosityShape &s, bool invert, int sel)`**: **Inputs:** the luminosity shape, `invert` and the mask's index. **Returns** what the mask selects, as a red bitmap. The weights come from the same `LuminosityShape::prepare` the renderer uses, run over the small picture as it stands just before this mask: the global adjustments plus the masks listed above it, but not this mask's own edits (which cannot change what it selects). Cached until the shape, the picture's pixels or the mask's place in the list change.

**`static void paintLuminosityTint(CanvasContext &ctx, const LuminosityShape &s, bool invert, int sel)`**: Draws `luminosityTintImage` over the whole picture.

**`static std::shared_ptr<CanvasImage> compositeTintImage(const Mask &m, int sel)`**: **Inputs:** a mask and its index. **Returns** the red bitmap of a selected mask with several parts (or an Object / Colour Range / Rectangle part): the whole combination evaluated by the same `MaskComposite` the renderer uses, on a small picture. If any part is a luminosity range or colour range the picture is the one just before this mask; otherwise the weights need no pixels and the bitmap is sized from the document. Cached on the parts in their in-view form, so a rotate / crop / edit rebuilds it.

**`static void paintCompositeTint(CanvasContext &ctx, const Mask &m, int sel)`**: Draws `compositeTintImage` over the whole picture.

**`static void paintSelectedObjectPrompts(CanvasContext &ctx)`**: Draws the selected object part's click points as dots (green = foreground, blue = background, white ring), taking the pending prompts while a segmentation is running, else the stored ones, converted to the picture as shown.

**`static void paintPartOutline(CanvasContext &ctx, const MaskPart &p, const GeoFrame &frame, bool selected)`**: **Inputs:** a part, the frame and whether it is selected. Draws the part's outline, handles or pin by its shape type; nothing for a luminosity range (its tint is the whole overlay).

**`static void paintMaskOverlay(CanvasContext &ctx)`**: The canvas calls this when the mask tool is active. Draws the **unselected** masks' outlines first (faint), then the **selected** one on top. A multi-part mask: the combined tint (if the tint is on), the other parts' outlines, the selected part's outline with handles, the object prompts and the brush cursor while painting. A single-part mask draws by shape: radial / linear: tint + outline with handles; brush: tint (own switch `brushTint`) + pin + cursor; luminosity and colour range: the tint only (no outline); object: tint + prompts; rectangle and polygon: combined tint + outline.

### `mask_widgets.hpp`
The row widgets of the Masks pane and, because they were written here, of the Presets, LUTs and Snapshots panes: the mask list row (`buildMaskRow`, `maskLabel`) and the three other list rows (`buildPresetRow`, `buildLutRow`, `buildSnapshotRow`). All use the same technique: the view tree is built **once** with a fixed number of slots (`kMaxMasks`, `kPresetRows`, `kLutRows`, `kMaxSnapshots`), and each row shows itself only for an index that currently exists, via polled `display` / label lambdas, so lists never rebuild views.

**Depends on:** `lut_pane.hpp`, `masks.hpp`, `presets.hpp`, `snapshots.hpp`; engine: `file_utils.hpp`, `lut3d.hpp`, `mask.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static std::string maskLabel(const Mask &m)`**: **Input:** a mask. **Returns** what it is called in the list: "Empty" for a mask with no parts; its custom name if it has one; otherwise its first part's kind, then the next one, then how many more ("Radial + Brush +1").

**`static View buildMaskRow(size_t i)`**: **Input:** a slot index. One row of the mask list: "1   Radial" (plus "(inverted)" and "(off)" where they apply), highlighted when selected. There are `kMaxMasks` of these, shown only for the masks that exist. Click selects the mask (`selectMask`); choosing a different mask also resets the selected part to 0.

**`static View buildPresetRow(size_t i)`**: **Input:** a slot index. One row of the Presets list. Click applies the preset (undoable). Shown only while the Presets pane is open and the row exists.

**`static View buildLutRow(size_t i)`**: **Input:** a slot index. One row of the LUTs list: the open picture under that LUT (the preview), the LUT's name and a line about the file (`lutRowDetail`). Click applies it (undoable). With no picture open a row is just the name.

**`static View buildSnapshotRow(size_t i)`**: **Input:** a slot index. One row of the Snapshots list. Click restores the snapshot (undoable).

### `history_panel.hpp`
The **History** panel (activity `kHistoryActivity`), and, because the code was written right after it, the row and slider builders of the Masks pane: part rows and the shape / adjustment slider specs. History: row r shows step `n − 1 − r`: newest at the top, the untouched picture at the bottom; there are `kMaxUndoSteps + 1` rows built once and shown only for steps that exist (the view tree is built once, like the mask list). Clicking a row jumps to that step (`jumpToHistory`), one history move that can be undone / redone like any other.

**Depends on:** `render_update.hpp`, `widgets.hpp`, `masks.hpp`; engine: `adjustments.hpp`, `geometry.hpp`, `mask.hpp`, `mask_geom.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static size_t historyStepForRow(size_t r)`**: **Input:** a row index. **Returns** the history step it shows (`n − 1 − r`), or `n` ("no such step") for a row past the end.

**`static void jumpToHistory(size_t step)`**: **Input:** a history step. Moves the editor to that step (`adjust.goToHistory`), drops the curve editors' selected points and refreshes the picture, the same follow-up as undo / redo. Ignored while dragging, for a step that doesn't exist, or for the current step.

**`static View buildHistoryRow(size_t r)`**: **Input:** a row index. One History row (a 26 px button): shown only while the History pane is open and the step exists; the current step is highlighted with a border; the label is `adjust.historyLabel(step)`. Click = `jumpToHistory`.

**`static View buildPartRow(size_t j)`**: **Input:** a part index. One row of the selected mask's part list: "1 Radial", "2 Subtract: Brush". Shown only while the mask has more than one part (a one-part mask looks exactly as it always did).

**`template <class Shape> static SliderSpec maskShapeSlider(const char *name, float lo, float hi, std::function<float(const Shape &)> get, std::function<void(Shape &, float)> set, int control)`**: **Inputs:** label, range, a getter and a setter on the shape itself, and the undo-coalescing control id. **Returns** a `SliderSpec` for the selected part's shape of type `Shape`.

**`static SliderSpec maskRadialSlider(const char *name, float lo, float hi, std::function<float(const RadialShape &)> get, std::function<void(RadialShape &, float)> set, int control)`**: `maskShapeSlider<RadialShape>`.

**`static SliderSpec maskLinearSlider(const char *name, float lo, float hi, std::function<float(const LinearShape &)> get, std::function<void(LinearShape &, float)> set, int control)`**: `maskShapeSlider<LinearShape>`.

**`static View buildMaskAdjustmentSlider(const AdjustmentSpec &spec)`**: The mask's own copy of one Adjust slider (Exposure, Contrast, ...): one per `kAdjustmentSpecs` entry, so a new global adjustment appears here too.

## Canvas painting, picture tools and the curve editors

### `crop_overlay.hpp`
Paints the crop selection over the picture while the Crop pane is open: the dimmed area outside the selection, the rule-of-thirds guides, the border and the eight drag handles. Drawn in image coordinates (the canvas is already scaled by `state.zoom`), so line widths are divided by the zoom to stay one screen pixel. The pointer logic is `crop_tool.hpp`.

**Depends on:** only `state`, `uiState` and the constants (see the top of this file). **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void paintCropOverlay(CanvasContext &ctx)`**: **Input:** the canvas context (image coordinates). Fills the four rectangles around the selection with black at alpha 120, draws the thirds guides (white, alpha 110), the border (white, 1.5 screen px) and handles of size `kCropHandlePx`: the corners always, and the side midpoints only when the selection is more than four handle-widths across on that axis.

### `compare.hpp`
**Before / After.** "Before" is `state.source`: the picture with its crop and rotation but none of the slider, curve, colour or mask edits. Keeping the geometry means the canvas doesn't change size, so the comparison is pixel for pixel. Two modes in `state.compare`: `Before` (the whole picture shows the original) and `Split` (a draggable vertical line: Before on the left, After on the right). The `// BEGIN compare` / `// END compare` fence is kept around the whole file.

**Depends on:** only `state`, `uiState` and the constants (see the top of this file). **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void toggleBefore()`**: View > Before: switches `state.compare` between `Before` and `Off` and repaints. No-op without a picture.

**`static void toggleSplit()`**: View > Split: switches `state.compare` between `Split` and `Off`. No-op without a picture.

**`static void splitFromX(float localX)`**: **Input:** an x in canvas-local pixels. Dragging in the canvas moves the split line: `splitPos = x / (docWidth × zoom)`, clamped to 0.02..0.98.

**`static void paintCompareLabel(CanvasContext &ctx, const std::string &text, float x, float y, bool alignRight)`**: **Inputs:** the text, a position and `alignRight`. A small dark pill (alpha 160, 22 px high) with white semi-bold text, its left (or, with `alignRight`, right) edge at `x`.

**`static void paintCompareOverlay(CanvasContext &ctx)`**: Screen-space decoration over the canvas while a compare mode is on: the "Before" / "After" labels and, in Split, the split line.

### `canvas_paint.hpp`
Painting the main canvas: `paintCanvas`, the one painter of the whole picture area. Background, the picture (edited, original or mid-drag preview, per the compare mode), the clipping warning, the crop or mask overlay, the compare labels and the scrollbars. Also the GPU-texture bookkeeping: the canvas draws up to three pictures (the edited image, the original `source` for Before and Split, and the mid-drag preview), and `drawImage` keeps **one** cached GPU texture per canvas, so alternating between them re-uploaded the whole picture every time. Each now has a `CanvasTexture` of its own, made lazily here the first time it is painted and replaced when the picture is.

**Depends on:** `canvas_view.hpp`, `crop.hpp`, `masks.hpp`, `mask_overlays.hpp`, `crop_overlay.hpp`, `compare.hpp`, `auto_tone_hist.hpp` (defined later); engine: `histogram.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `struct PictureTex`: A GPU texture (`tex`) and the image it was made from (`of`): the texture is replaced when `of` changes.

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `of` | `std::shared_ptr<const CanvasImage>` |  | the picture `tex` shows (held, so its address cannot be reused) |
  | `tex` | `std::shared_ptr<CanvasTexture>` |  |  |


**State and tables**

- `gImageTex`, `gSourceTex`, `gPreviewTex`: `static PictureTex`. The three picture-texture slots: the edited image, the original (`source`, for Before and Split) and the mid-drag preview. Mutated during paint, on the UI thread (not part of `uiState`, which holds no GPU objects).

**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static void computeHistogram()`: Red/blue clipping warning over the edited picture. The overlay image is in document space (stretched to docW x docH like the preview), so it lines up at any zoom. computeHistogram builds it.

**Functions**

**`static void paintClipOverlay(CanvasContext &ctx, float docW, float docH)`**: Draws the clipping warning bitmap (`state.clipOverlay`) over the picture when either clipping switch is on, recomputing the histogram first if it is stale. Nothing when both are off.

**`static const std::shared_ptr<CanvasTexture> & pictureTexture(PictureTex &slot, const std::shared_ptr<CanvasImage> &img)`**: **Inputs:** a `PictureTex` slot and the image. **Returns** the GPU texture for it, creating it on first use and replacing it when the picture changed. The image and the source are built once and replaced, never edited in place (the editor's rule, see architecture.md), so the texture can **share** their pixels: `wrap()` costs no extra memory. Null if there is no picture or it can't be wrapped; the caller then falls back to `drawImage`.

**`static const std::shared_ptr<CanvasTexture> & previewTexture(const std::shared_ptr<CanvasImage> &img)`**: **Input:** the preview image. **Returns** a texture for the drag preview. The preview is a **new** small image (about 1 MP) every frame, so wrapping each would still create and delete a GPU texture per frame. One texture of the preview's size is kept instead and each new preview is copied into it (a few MB), which uploads as a sub-image into the same GPU texture.

**`static void paintCanvas(CanvasContext &ctx)`**: The canvas painter. With no picture it clears the three textures and fills the placeholder background. Otherwise: viewport background, the picture's rectangle (white, or the empty-corner colour when straighten has expanded the canvas), clip to the picture, translate to its origin, scale by `state.zoom`. The **edited** picture is the mid-drag / awaiting-render preview stretched to the document size when `state.showPreview()`, else `state.image`; the **original** is `state.source` (the edited picture if there is none). Then by `state.compare`: `Before` draws the original; `Split` draws the original left of `splitPos` and the edited picture right of it, each under its own clip; `Off` draws the edited picture, the clipping overlay, and the crop overlay (Crop pane) or the mask overlay (Masks pane). Back in screen space: the compare labels and line, a one-device-pixel hairline just outside the picture, and the scrollbars (`paintViewBars`). Profiled with `LITEUI_PROF_*`.

### `readout.hpp`
The RGB readout under the cursor (shown in the status bar): samples the picture the canvas is showing at the pointer and stores it in `state.hoverValid` / `hoverR` / `hoverG` / `hoverB`. Both functions return true only when the readout changed, so LiteUI repaints only then (a change marks the canvas dirty).

**Depends on:** only `state`, `uiState` and the constants (see the top of this file). **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static bool hoverClear()`**: Clears the readout. **Returns** true if it was showing something.

**`static bool hoverSample(float lx, float ly)`**: **Inputs:** the pointer in canvas-local pixels. Samples the picture the canvas is showing: the original in Before (or the left half of Split), else the edited image or the mid-drag preview. Leaves the readout alone during a drag; clears it when the pointer is off the picture or on a transparent pixel (straighten's empty corner). **Returns** true only when the stored values changed.

### `wb_picker.hpp`
The **white-balance picker**, armed from the Adjust pane's button or the **W** key. The maths is the engine's `solveWhiteBalance` (`color_pass.hpp`); this is only the UI side: reading the spot under the click and reporting the result. The click is sampled from `state.source` (what the adjustments run on) through the current tone tables, before any white balance, so the result is the same whatever the white balance currently is.

**Depends on:** `geometry_worker.hpp`; engine: `adjustment_set.hpp`, `color_pass.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

Armed from the Adjust pane's button or the W key. Its math is solveWhiteBalance (color_pass.hpp); this is only the UI side: reading the spot under the click and reporting the result. Half-width of the square patch averaged around the click: 2 = 5 x 5 pixels, so one noisy pixel cannot decide the result.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kWbPatchRadius` | `int` | `2` | Half-width of the square patch averaged around the click: 2 = 5 × 5 pixels, so one noisy pixel cannot decide the result |

**Functions**

**`static bool wbPickerActive()`**: True while the picker is armed and still makes sense. It only works in the Adjust pane (activity 1) on a shown picture, outside Before / After; once any of those stops being true it disarms itself, so it can never wake up later.

**`static void toggleWbPicker()`**: Arms or disarms the picker (the button, the W key). Ignored while dragging. Disarming clears the status. Arming needs a picture; inside Before / After it refuses ("Leave Before / After to pick a white balance"). Arming opens the Adjust pane (activity 1) and says "White balance: click a neutral gray in the picture (Esc cancels)".

**`static bool wbSampleSpot(float lx, float ly, float out[3])`**: **Inputs:** a canvas-local point and an out-array of 3 floats. **Returns** true and the average colour of the square patch (radius `kWbPatchRadius`, so 5×5 pixels) around the point in `state.source`, run through the current tone tables (`buildChannelLuts`), skipping transparent pixels. False if the point is outside the picture or the patch has no visible pixel (straighten's empty corners).

**`static bool wbPress(float lx, float ly)`**: A click while the picker is armed. **Returns** true if it consumed the press (so the crop and mask tools never see it). While a geometry job runs: "Wait for the changes to finish applying". A click outside the picture does nothing and keeps the picker armed. `solveWhiteBalance` then decides: a **clipped** spot ("That spot is clipped (pure white): pick a gray that is not blown out") and a spot **too dark** ("...too dark to read a color: pick a brighter gray") keep the picker armed so you can try again. A usable pick disarms the picker and sets Temperature and Tint with `adjust.setWhiteBalance` (one undo step): "White balance: Temperature +N, Tint +M", with "(that spot cannot be made fully gray)" when the solution was limited; "White balance is already set for that spot" if nothing changes.

### `auto_tone_hist.hpp`
Three related things the canvases poll or paint together. **Auto Tone** (the Adjust pane's button, the **A** key and Image > Auto Tone): the maths is `solveAutoTone` (`core/auto_tone.hpp`), the edit is `AdjustmentSet::setAutoTone` (Exposure, Contrast, Whites and Blacks in one undo step); this is only the UI side, measuring the picture and reporting the result. It is measured on `source`, the picture the adjustments run on, **not** on the shown picture: the shown one already has the tone sliders applied, so its histogram would change every time Auto Tone ran; reading `source` makes the answer the same whatever the sliders say, so pressing it twice is a no-op. **`flushPendingAdjust`**: the per-frame poll that collects every background job and runs the deferred recompute. **The histogram / scopes** pinned canvas: computing the data from the picture on screen and painting the histogram bars, the waveform or vectorscope, and the clipping triangles. The counting and the overlay maths are the engine's `histogram.hpp` and `scopes.hpp`.

**Depends on:** `render_update.hpp`, `geometry_worker.hpp`, `rotate.hpp`, `segmentation.hpp`, `saved_edits.hpp`, `lut_pane.hpp`, `sync.hpp`, `export.hpp`, `meta_commands.hpp`; engine: `adjustment_set.hpp`, `auto_tone.hpp`, `histogram.hpp`, `scopes.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Forward declarations** (defined in a later file; declared here because this code is written before it)

- `static void refreshHistogramIfStale()`: Brings the shown image (and, if stale, the histogram) up to date. Several canvases poll this; only the first poll of a frame does any work.

**Functions**

**`static void autoTone()`**: Ignored while dragging or without a source. While a geometry job runs: "Wait for the changes to finish applying". Counts the histogram of `state.source`, runs `solveAutoTone` on the luma counts and reports: "nothing to measure in this picture" (no pixels), "this picture has almost no tonal range to adjust" (flat), "the tone is already set for this picture" (no change), otherwise sets the four values (one undo step), recomputes, and says "Auto Tone: Exposure +N, Contrast +N, Whites +N, Blacks +N", with "(the slider ranges are not enough to go further)" when the solution was limited.

**`static void flushPendingAdjust()`**: **The per-frame poll.** Idempotent: the main canvas and the histogram canvas both call it from their dirty polls, and whichever is polled first (the sidebar, so usually the histogram) does the work, so the histogram never lags a frame behind the picture. In order it collects every finished background job (`collectFullRender`, `collectGeometryJob`, `collectDecodeJob`, `collectSegmentJob`, `collectSyncJob`, `collectExportJob`), takes in LUT previews and starts a new batch if needed (`pollLutPreviews`), and autosaves the sidecar once the mouse is released (`autosaveSidecar`). Then, if `adjustDirty` is set, it clears it and runs the deferred, expensive part of a slider drag exactly once per pending change: `updateAnglePreview()` for a mid-drag angle change, `updatePreviewFromAdjustments()` for any other mid-drag change (both cheap, on the small preview), else `updateImageFromAdjustments()` (full resolution).

**`static void computeHistogram()`**: Rebuilds the R / G / B / luma histograms (`histR` ... `histL`), the shadow and highlight clip counts, the clipping overlay (with the current switches and per-channel mode) and the scope picture (`scopes::render` for `state.scopeKind`) from the image currently on screen (the mid-drag preview, or the full-resolution result). Clears `histStale` and bumps `histVersion`, which is what the histogram canvas's dirty poll watches.

**`static void paintHistogramBars(CanvasContext &ctx, float W, float H)`**: **Inputs:** the context and the canvas size. Luma as grey bars, R / G / B as thin coloured outlines on top. Bar heights use square-root scaling and are normalized to the tallest bin among 1..254, so a pile of clipped pixels in bin 0 / 255 doesn't flatten the rest of the graph.

**`static void paintClipTriangles(CanvasContext &ctx, float W)`**: The two clipping triangles in the top corners of the pinned canvas: left = shadows, right = highlights, lit when that overlay is on. They stay in every view (histogram, waveform, vectorscope): clipping is a property of the picture, not of the way it is plotted.

**`static void paintScope(CanvasContext &ctx, float W, float H)`**: Waveform or vectorscope: the picture `scopes::render` made, stretched over the canvas (the waveform fills it, the vectorscope is a centred square), plus a faint graticule and the view's name in the bottom-left corner.

**`static void paintHistogram(CanvasContext &ctx)`**: The pinned canvas's painter: records the canvas width in `uiState.hist.canvasW` (for the corner hit-test), then draws the histogram, or the waveform / vectorscope picked with a click (`cycleScope`), always with the clipping triangles on top.

**`static void refreshHistogramIfStale()`**: Runs `flushPendingAdjust()`, then `computeHistogram()` if the histogram is stale. Several canvases poll this; only the first poll of a frame does any work.

**`static void setClipping(bool shadows, bool highlights)`**: **Inputs:** shadows on / off, highlights on / off. Sets the switches and recomputes right away: this rebuilds the overlay, bumps `histVersion` (so the histogram repaints its triangles) and tells the canvas to repaint.

**`static void toggleClipping()`**: **J:** both overlays on, or both off if either is currently on.

**`static void toggleChannelClipping()`**: **Shift+J:** switch between classic red / blue and per-channel colours. If no overlay is on yet, turns both on so the key always shows something.

**`static void cycleScope()`**: A click on the pinned canvas (outside the clipping corners): Histogram → Waveform → Vectorscope → Histogram. `computeHistogram()` rebuilds the scope picture for the new view and bumps `histVersion`, so the canvas repaints; its height follows `scopeKind` (see where the canvas is built, `buildHistogramView`).

### `curve_editor.hpp`
The canvas side of the **Curve** pane (the tone curve editor): converting between canvas pixels and curve units, picking and dragging points, switching and resetting channels, and painting the plot. The curve maths (inserting, moving, removing points, the spline) is the engine's `curves.hpp`; the edits go through `AdjustmentSet::setCurve`. Curve units are 0..255 with y pointing up; the canvas is square with `kCurvePadPx` of inset. The selected channel, the selected point and the grab offset are `uiState.curve.*` (`ui_state.hpp`); the selected point is dropped by `uiState.dropCurveSelection()` whenever the curves may have been replaced (undo, a preset, a snapshot, a new picture).

**Depends on:** `render_update.hpp`; engine: `adjustment_set.hpp`, `curves.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static float curvePlotPx()`**: **Returns** the side of the square plot area in canvas-local pixels (the canvas size minus the inset on both sides). The mapping between canvas-local screen px and curve units (0..255, y up) is `curveFromLocal` / `curveToLocal`.

**`static CurvePoint curveFromLocal(float lx, float ly)`**: **Inputs:** a canvas-local point. **Returns** the curve point (x, y in 0..255, y up), clamped to the plot.

**`static void curveToLocal(float x, float y, float &lx, float &ly)`**: **Inputs:** a curve point; out-references. The inverse of `curveFromLocal`: the canvas-local pixel position.

**`static int curveHit(const CurvePoints &pts, float lx, float ly)`**: **Inputs:** a curve and a canvas-local point. **Returns** the index of the point within `kCurveHitPx` of it (the nearest wins), or -1.

**`static const CurvePoints &curveActiveCurve()`**: **Returns** the curve being edited: `adjust.curve(uiState.curve.channel)`.

**`static void curvePress(float lx, float ly)`**: Pointer press at canvas-local `(lx, ly)`. On an existing point: selects it and records the offset between the point and the pointer (`grabDx/Dy`, so the point doesn't jump to the pointer). Elsewhere: inserts a new point there (`curveInsert`; refused at `kCurveMaxPoints`), commits it (coalesced) and selects it. Marks `uiState.curve.uiDirty` so the canvas repaints. No-op without a picture.

**`static void curveDrag(float lx, float ly)`**: Pointer drag: moves the selected point to the pointer plus the grab offset (`curveMovePoint`, which keeps the points in order and in range). If the curve didn't change it returns; otherwise commits it with `setCurve(..., coalesce = true)` (one undo step per drag), enters the preview path and asks for the deferred recompute.

**`static void curveEnd()`**: Pointer release: deselects the point, repaints, closes the coalesced curve edit (`adjust.endCurveEdit()`) and, if a drag was running, ends it and asks for one full-resolution render.

**`static void curveRightPress(float lx, float ly)`**: Right-click on a point removes it (`curveRemove`; the end points can't be removed), as its own undo step. No-op without a picture or when no point is under the pointer.

**`static void setCurveChannel(CurveChannel ch)`**: **Input:** a `CurveChannel`. Switches the edited channel (RGB master, Red, Green, Blue), deselecting the point.

**`static void resetCurve()`**: Resets the curve being edited to the identity line (one undo step).

**`static void resetAllCurves()`**: Resets all four curves (one undo step).

**`static const char *curveChannelName(CurveChannel ch)`**: **Returns** "RGB" (master), "Red", "Green" or "Blue".

**`static Color curveChannelColor(CurveChannel ch, uint8_t alpha)`**: **Inputs:** a channel and an alpha. **Returns** the line / tint colour: white for the master, the channel's own colour otherwise.

**`static const std::array<uint32_t, 256> &curveHistogram(CurveChannel ch)`**: **Returns** the histogram that matches a curve: luma for the master, else that channel's.

**`static void paintCurve(CanvasContext &ctx)`**: The canvas painter: a dark plot with the edited channel's histogram faintly behind it, a quarter grid, the identity diagonal, every non-straight curve (the others dimmed in their own colour), and the edited curve's control points (the selected one highlighted).

### `mask_effects.hpp`
The **Effect tab** of the right-hand mask panel: Amount, Colour Mix, Colour Grading (per range plus Blending / Balance) for the **selected** mask. Everything here edits the selected mask through `maskEdit`, so it is undoable and a drag folds into one undo step. The `kMaskCtl*` ids below are the "control" part of that undo key; they must stay below `kMaskCtlStride` and differ from the `kMaskCtl*` ids in `masks.hpp`, which a compile-time check enforces. The panel itself is assembled in `panes/masks_pane.hpp`; the mask's Adjust sliders are `buildMaskAdjustmentSlider` (`history_panel.hpp`) and its tone curve `mask_curve.hpp`.

**Depends on:** `widgets.hpp`, `masks.hpp`; engine: `adjustments.hpp`, `color_grade.hpp`, `color_mix.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Constants**

The Effect tab of the right-hand mask panel. Everything here edits the SELECTED mask through maskEdit, so it is undoable and a drag folds into one step. The ids below are the "control" part of that undo key: they must stay below kMaskCtlStride and differ from the kMaskCtl* ids above.

| Name | Type | Value | Meaning |
|---|---|---|---|
| `kMaskCtlAmount` | `int` | `6` | Undo control id of the Amount slider |
| `kMaskCtlMix` | `int` | `30` | + chan * kMixBandCount + band (30..53) |
| `kMaskCtlGrade` | `int` | `60` | + field * 4 + range (60..71) |
| `kMaskCtlBlend` | `int` | `72` | Undo control id of Blending |
| `kMaskCtlBalance` | `int` | `73` | Undo control id of Balance |
| `kMaskCtlCurve` | `int` | `80` | + channel (80..83) |
| `kMaskCurveSizePx` | `float` | `240.0f` | Side of the mask curve canvas, in px |

**Compile-time check:** `static_assert` that the control ids of one mask, taken as inclusive ranges (shape sliders, Amount, Mix `30..30+kMixChanCount×kMixBandCount−1`, Grade `60..71`, Blend, Balance, Curve `80..83`, Range, Name, the drag ids, Brush, Luminosity, Polygon, and `kMaskCtlAdj .. +AdjId::Count−1`), neither overlap nor leave `0..kMaskCtlStride−1`; otherwise two different controls would fold into one undo step. (`Adj` used to be `10 + AdjId`, which overlapped the Colour Mixer ids.)

**Functions**

**`static std::string maskIntText(float v)`**: **Input:** a value. **Returns** it rounded to a whole number as text.

**`static std::string maskSignedText(float v)`**: **Input:** a value. **Returns** it rounded, with a leading "+" when positive.

**`static View buildMaskAmountSlider()`**: Amount, 0..100: how strongly all of this mask's adjustments apply (Lightroom's mask "Amount"). 100 = full, 0 = the mask does nothing. Edits `mask.amount` with `maskEdit(..., kMaskCtlAmount)`; the drag ends with `adjust.endMaskEdit()`.

**`static View buildMaskMixSlider(MixChan chan, size_t band)`**: **Inputs:** the channel and the band. One Colour Mixer slider (a band's Hue / Saturation / Luminance) of the mask, tinted in the band's colour; control id `kMaskCtlMix + chan × kMixBandCount + band`.

**`static View buildMaskGradeSlider(const char *name, int range, float GradeWheel::*field, int fieldIdx, float lo, float hi, bool isSigned)`**: **Inputs:** label, the grading range, a pointer-to-member for the field (hue / saturation / luminance), the field index, range and signed flag. One hue / saturation / luminance slider of one grading range of the mask. `fieldIdx` (0 hue, 1 sat, 2 lum) only keeps the undo keys apart (`kMaskCtlGrade + field × 4 + range`).

**`static View buildMaskGradeShared(bool balance)`**: **Input:** `balance`. Blending (0..100) or Balance (−100..100), shared by the mask's ranges (control ids `kMaskCtlBlend` / `kMaskCtlBalance`).

### `mask_curve.hpp`
The canvas side of the **Curve tab** of the mask panel: the same behaviour as the Curve pane (click = add a point, drag = move, right-click = delete) but on the **selected mask's own curves**, and a bit larger (`kMaskCurveSizePx`). It is a copy of `curve_editor.hpp` working on `mask.curves[channel]` and on `uiState.maskPane.*`, with a `mc` prefix. The selected channel, point and grab offset are `uiState.maskPane.curveCh` / `curveActive` / `curveGrab*`.

**Depends on:** `render_update.hpp`, `widgets.hpp`, `masks.hpp`, `curve_editor.hpp`, `mask_effects.hpp`; engine: `curves.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static float mcPlotPx()`**: **Returns** the side of the square plot in canvas-local pixels (`kMaskCurveSizePx` minus the inset on both sides).

**`static CurvePoint mcFromLocal(float lx, float ly)`**: Canvas-local point → curve point (0..255, y up), clamped; as `curveFromLocal` but for the larger canvas.

**`static void mcToLocal(float x, float y, float &lx, float &ly)`**: Curve point → canvas-local pixel position; the inverse of `mcFromLocal`.

**`static int mcHit(const CurvePoints &pts, float lx, float ly)`**: **Returns** the index of the point within `kCurveHitPx` of the pointer (the nearest wins, compared by squared distance), or -1.

**`static CurvePoints maskCurveCopy()`**: **Returns** a copy of the curve being edited (`mask.curves[uiState.maskPane.curveCh]`), or an empty list if no mask is selected.

**`static void maskCurveCommit(const CurvePoints &pts, bool coalesce)`**: **Inputs:** the new point list and `coalesce`. Stores it on the selected mask through `maskEdit(..., kMaskCtlCurve + channel)`. `coalesce = true` folds a drag into one undo step; false = its own step.

**`static void maskCurvePress(float lx, float ly)`**: Pointer press: on an existing point, selects it and records the grab offset; elsewhere inserts a new point (refused at `kCurveMaxPoints`), commits it (coalesced) and selects it. Marks `curveUiDirty` so the canvas repaints. No-op without a picture or a selected mask.

**`static void maskCurveDrag(float lx, float ly)`**: Pointer drag: moves the selected point (`curveMovePoint`) to the pointer plus the grab offset; if the curve changed, commits it (coalesced), enters the preview path and asks for the deferred recompute.

**`static void maskCurveEnd()`**: Pointer release: deselects the point, repaints, closes the coalesced edit (`adjust.endMaskEdit()`) and, if a drag was running, ends it and asks for one full-resolution render.

**`static void maskCurveRightPress(float lx, float ly)`**: Right-click on a point removes it (its own undo step).

**`static void paintMaskCurve(CanvasContext &ctx)`**: The painter: a dark plot, a quarter grid, the identity diagonal, then the selected mask's curves (the edited channel bright, the others dimmed in their own colour) and the edited curve's control points. Draws nothing more when no mask is selected.

**`static View buildMaskCurveCanvas()`**: **Returns** the curve canvas: a square of `kMaskCurveSizePx`, with the painter, the drag gesture (`bindDrag`: press, drag, end, right-press, crosshair cursor) and a **dirty source**: it repaints only when `adjust.version()`, the selected mask, the active tabs (`maskTab × 10 + maskFx`) or `curveUiDirty` changed (that flag is consumed here). The "last seen" values are function-local statics, so there must be only one such canvas.

### `startup_checks.hpp`
The one check that runs once at startup, before the window exists.

**Depends on:** engine: `adjustments.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void verifyAdjustmentSpecs()`**: The pipeline indexes `kAdjustmentSpecs` by `AdjId` (the tone tables, the sidebar loop, the colour pass), so entry i must describe `AdjId` i. This can't be a `static_assert`: the table holds `std::function` and `std::string` members, which aren't usable in constant expressions. So it is checked once at startup, and a mistake aborts with a message naming the bad entry rather than silently moving the wrong slider.

## Panes (`panes/`)

### `panes/pane_helpers.hpp`
The one small view shared by several panes: the muted section heading.

**Depends on:** only `state`, `uiState` and the constants (see the top of this file). **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static Text sectionLabel(const char *text)`**: A small muted heading above a group of controls (Adjust sections, mask shape sections).

### `panes/info_pane.hpp`
The **Info** pane (activity 0): real data about the open picture, one text row each. **File**, **Size** (px), **Zoom** (%), **Color** (the colour-space note, if any), and then the camera data from the file's EXIF block: **Camera**, **Lens**, **Exposure**, **Focal length**, **Taken**. Each camera row hides itself when the file doesn't say; a picture with none of it gets one muted line ("No camera data in this file") instead. GPS is never shown. Without a picture the pane says "No image open". The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `saved_edits.hpp`; engine: `exif.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildInfoPane()`**: **Returns** the Info pane (shown while `activeActivity == 0`). Rows are polled text labels reading `state.imagePath`, `state.image`, `state.zoomDev`, `state.colorTag` and `state.photoInfo`.

### `panes/history_pane.hpp`
The **History** pane (activity `kHistoryActivity`, 9): every undo step, newest first, click to jump (`buildHistoryRow`, `jumpToHistory`). A hint line explains that dimmed steps are ahead of you and that a new edit discards them ("Open an image to see its edit history." without a picture). The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `history_panel.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildHistoryPane()`**: **Returns** the History pane (shown while `activeActivity == kHistoryActivity`): the hint line, then `kMaxUndoSteps + 1` `buildHistoryRow` slots.

### `panes/snapshots_pane.hpp`
The **Snapshots** pane (activity `kSnapshotsActivity`, 11): named copies of every edit on this picture. Click one to restore it (one undo step); type a name and press **Save snapshot** (Enter in the field also saves) to keep the current state. A hint explains what a snapshot contains; an "empty" line appears when there are none; one row per slot (`buildSnapshotRow`, up to `kMaxSnapshots`); **Delete selected snapshot**. They are saved in the picture's edits file, not in the presets folder. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `widgets.hpp`, `snapshots.hpp`, `mask_widgets.hpp`; engine: `sidecar.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildSnapshotsPane()`**: **Returns** the Snapshots pane (shown while `activeActivity == kSnapshotsActivity`). The name field is a `TextInput` whose shared state is kept in `uiState.names.snapshot` and registered with `registerTextField`; its text is filtered by `filterSnapshotNameField`.

### `panes/adjust_pane.hpp`
The **Adjust** pane (activity 1): one slider row per entry in `kAdjustmentSpecs`, grouped by stage with `sectionLabel` headings: **Tone** (with the **Auto Tone (A)** button), **Color** (with **Pick White Balance (W)** above Temperature, the first slider it sets), **Filters**, **Detail**, **Effects** (Vignette and Grain, the last step of the render, after the masks). Adding a new adjustment later is a one-line addition to that table; this loop, the render pass (`renderAdjusted`) and `buildAdjustmentSlider` need no changes. LUT Amount is not here: it is the LUTs pane's slider (it only scales the chosen LUT). The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `widgets.hpp`, `wb_picker.hpp`, `auto_tone_hist.hpp`, `panes/pane_helpers.hpp`; engine: `adjustments.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildAdjustPane()`**: **Returns** the Adjust pane (shown while `activeActivity == 1`). Walks `kAdjustmentSpecs` in order, inserting a `sectionLabel` at the first entry of each stage and skipping `AdjId::LutAmount`.

### `panes/presets_pane.hpp`
The **Presets** pane (activity `kPresetsActivity`, 10): saved looks. Click one to apply it (one undo step); type a name and press **Save current look** to store the current look; **Delete selected preset**. A hint explains that a preset holds sliders, curves, Colour Mix, Colour Grading and the LUT but not crop, rotation or masks. One row per slot (`buildPresetRow`, up to `kPresetRows`), an "empty" line, and "Saved in <presetDir>". The list is re-read from disk each time the pane is opened. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `widgets.hpp`, `presets.hpp`, `mask_widgets.hpp`; engine: `preset.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildPresetsPane()`**: **Returns** the Presets pane (shown while `activeActivity == kPresetsActivity`). Re-reads the folder (`refreshPresets`) the first time the pane is shown after being hidden, from a polled style (a function-local `wasShown` flag); the name field's state is `uiState.names.preset`.

### `panes/luts_pane.hpp`
The **LUTs** pane (activity `kLutsActivity`, 12): 3D colour lookup tables (`.cube` files made for ordinary sRGB / Rec.709 pictures). **Import LUT...** copies files into the library folder; click a row to apply it (one undo step); the **LUT Amount** slider sets its strength; **Remove LUT**. A line says which LUT the picture uses and whether there is a problem with it (`lutCurrentText`). Each row previews the open picture under that LUT (made on a background worker, see `pollLutPreviews`); "No LUTs yet. Import a .cube file." when the library is empty; "LUT library: <folder>". The list is re-read each time the pane is opened. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `lut_pane.hpp`, `widgets.hpp`, `mask_widgets.hpp`; engine: `adjustments.hpp`, `lut_library.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildLutsPane()`**: **Returns** the LUTs pane (shown while `activeActivity == kLutsActivity`). Re-lists the library (`refreshLuts`) the first time the pane is shown after being hidden, from a polled style (a function-local `wasShown` flag); the LUT Amount slider's setter is `setLutAmount`.

### `panes/curve_pane.hpp`
The **Curve** pane (activity 4): tone curves, RGB (master) plus per-channel R, G, B, one tab each. A bullet after a tab name marks a curve that has been changed, so an edit on a channel you've since switched away from isn't forgotten. The plot is a canvas (`paintCurve`, `curvePress` / `curveDrag` / `curveEnd` / `curveRightPress`): click to add a point, drag to move it, right-click a point to delete it; the whole gesture is one undo step. The edited channel's histogram sits behind the curve, and other channels' curves that have been changed stay visible, dimmed. Hint lines, **Reset Curve** and **Reset All Curves**. The canvas repaints when the histogram version, the edit version, the channel or `uiState.curve.uiDirty` changed. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `widgets.hpp`, `auto_tone_hist.hpp`, `curve_editor.hpp`; engine: `curves.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildCurvePane()`**: **Returns** the Curve pane (shown while `activeActivity == 4`). The tabs call `setCurveChannel`; the canvas's dirty source is polled every frame and keeps its last-seen values in function statics (it also calls `refreshHistogramIfStale`).

### `panes/mix_pane.hpp`
The **Color Mix** pane (activity 5): an HSL mixer. Eight colour bands, each with Hue, Saturation and Luminance; the three tabs pick which of the three sets of eight sliders is shown (`uiState.mix.chan`; a bullet marks a tab that has been changed). One group of eight `buildMixSlider`s per tab, only the selected tab's displayed. **Reset Color Mix**. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `widgets.hpp`, `color_ui.hpp`; engine: `color_mix.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildMixPane()`**: **Returns** the Color Mix pane (shown while `activeActivity == 5`).

### `panes/grade_pane.hpp`
The **Color Grade** pane (activity 6): 3-way and single-range colour wheels (Shadows, Midtones, Highlights, Global) plus shared **Blending** and **Balance** sliders. Mode tabs: 3-Way | Shad | Mid | High | Glob (`uiState.grade.mode`; a bullet marks a range that has been adjusted). A readout line (`H:13 S:72 L:+1`) shows the values of the wheel touched last (`uiState.grade.active`). One body per mode (`buildGradeThreeWay`, `buildGradeSingle`), only the selected one displayed. **Reset Color Grading**. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `widgets.hpp`, `color_ui.hpp`; engine: `color_grade.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildGradePane()`**: **Returns** the Color Grade pane (shown while `activeActivity == 6`). The Blending and Balance sliders edit the grade through `gradeEdit` and end with `adjust.endGradeEdit()`.

### `panes/masks_pane.hpp`
The **Masks** pane (activity 7) and the editor of the selected mask, built together because they share many locals. Local adjustments: each mask is a shape plus its own copy of the Adjust sliders, applied only where the shape is. **In the sidebar** (the returned `pane`): a hint line, eight add buttons (**+ Radial Gradient**, **+ Linear Gradient**, **+ Rectangle**, **+ Polygon / Lasso**, **+ Brush**, **+ Luminosity Range**, **+ Object**, **+ Color Range**, each `addMask(type)`, dimmed without a picture) and the mask list (`kMaxMasks` `buildMaskRow` slots); the selected mask is drawn on the image, where it can be dragged (inside = move, square handles = resize). **In the right-hand panel** (the returned `editor`, hidden without a selected mask; placed by `addMaskPanel`): the **On / Invert / Tint** chips, the name field, the overlay-colour chips (one per `kMaskTintPalette` entry), the brush settings (they apply to the *next* stroke), then a tab bar (`state.maskTab`) with three tab bodies: **Effect** (a second row of chips, `state.maskFx`, picks Basic / Mix / Grade / Curve; a bullet marks a section changed for this mask), **Shape** (sliders per mask type, shown only for that type: radial, linear, rectangle, polygon / lasso, luminosity range with Min never passing Max, object, colour range) and **Parts** (combine several shapes into this mask; each part after the first is joined to the ones before it with Add / Subtract / Intersect; the part rows only appear once there is more than one). Below the tabs: the list operations (**Duplicate**, **Move Up**, **Move Down**, **Copy Effects**, **Paste Effects**; order matters, masks apply top to bottom) and **Delete Mask**. Effect > Basic is Amount plus the same table as the Adjust pane (via `buildMaskAdjustmentSlider`), except Vignette, Grain and the 3D LUT, which are global only (`renderWith` applies them after the masks). Effect > Mix, Grade and Curve use `mask_effects.hpp` and `mask_curve.hpp`. The chip builders (`maskChip`, `chipRow`) and the tab views are locals shared by all sections of this one function, which is why it is not split further yet. The function returns both views (`MaskPaneViews`); `buildSidebar` adds `pane` to the sidebar and hands `editor` back to `main()`, which gives it to `addMaskPanel`; it `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `segmentation.hpp`, `widgets.hpp`, `masks.hpp`, `virtual_copies.hpp`, `mask_list.hpp`, `mask_widgets.hpp`, `history_panel.hpp`, `curve_editor.hpp`, `mask_effects.hpp`, `mask_curve.hpp`, `panes/pane_helpers.hpp`; engine: `adjustments.hpp`, `color_grade.hpp`, `color_mix.hpp`, `curves.hpp`, `geometry.hpp`, `mask.hpp`, `mask_brush.hpp`, `mask_geom.hpp`, `mask_object.hpp`, `mask_polygon.hpp`, `mask_range.hpp`, `mask_rect.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Types**

- `struct MaskPaneViews`: The Masks pane (goes in the sidebar) and the editor for the selected mask (goes in the right-hand panel, built later: see addMaskPanel).

  | Field | Type | Default | Meaning |
  |---|---|---|---|
  | `pane` | `View` |  |  |
  | `editor` | `View` |  |  |


**Functions**

**`static MaskPaneViews buildMaskPanes()`**: **Returns** a `MaskPaneViews { pane, editor }`. The sidebar `pane` is shown while `activeActivity == 7`; the `editor` follows the selected mask (hidden without one).

### `panes/crop_pane.hpp`
The **Crop** pane (activity 2): aspect-ratio presets (one `sidebarButton` per `kCropAspects` entry, radio-style), a live readout ("Selection: W x H"), and the **Apply Crop**, **Reset Selection** and **Remove Crop** buttons. The selection itself is drawn and dragged directly on the canvas (`paintCropOverlay`, `cropPress`, `cropDrag`). A hint line explains the gestures ("Open an image to crop it." without a picture). The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `crop.hpp`, `geometry_worker.hpp`, `crop_tool.hpp`, `widgets.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildCropPane()`**: **Returns** the Crop pane (shown while `activeActivity == 2`). The aspect buttons call `setCropAspect(i)` and are highlighted while `state.crop.aspectIdx == i`; **Apply Crop** and **Remove Crop** are dimmed while a geometry job runs (`geometryBusy`).

### `panes/rotate_pane.hpp`
The **Rotate** pane (activity 3): four 90-degree turn / mirror buttons and **Reset Turns & Flips** (lossless: pure pixel moves, each press one undo step), then **Straighten**: an Angle slider (`setAngleFromSlider`, with the live preview path), fine step buttons right under it (the track is about 0.6 degrees per pixel, too coarse for levelling a horizon), **Corners** (**Auto-crop empty corners** or **Keep full canvas**, `setCornerMode`) and **Reset Angle**. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `geometry_worker.hpp`, `rotate.hpp`, `widgets.hpp`; engine: `geometry.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildRotatePane()`**: **Returns** the Rotate pane (shown while `activeActivity == 3`). The turn and flip buttons call `applyGeoOp`; the turn and flip buttons are dimmed while a geometry job runs (`geoIdle`, i.e. `!geometryBusy()`).

### `panes/export_pane.hpp`
The **Export** pane (activity 8): format buttons (**PNG (lossless)**, **PNG 16-bit**, **JPEG**), the JPEG options (**Quality** slider, **Keep EXIF & color profile**, **Remove location (GPS)**, only meaningful with the metadata kept so dimmed otherwise), **Resize to long edge** with a **Long edge** slider and a line saying the resulting size ("Exports at W × H" or "Already within N"), then **Export...** (single picture, `exportImage`; the File menu's Export... and Ctrl+E use the same settings) and a **Batch** group: **Overwrite existing files**, **Export Selected...**, **Export All Shown...**, **Stop Export**, with progress text ("Exporting N of M") while it runs. Resize applies to both formats, so it sits outside the JPEG-only options. The function returns the finished pane; `buildSidebar` (`shell/sidebar.hpp`) adds it to the sidebar. Like every pane it is built **once**: it is always in the view tree and shows itself only while its activity is selected, through a polled `display` style (see the note at the top of this file).

**Depends on:** `export.hpp`, `meta_commands.hpp`, `widgets.hpp`; engine: `raster_ops.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildExportPane()`**: **Returns** the Export pane (shown while `activeActivity == 8`). The format buttons set `state.exportJpeg` / `exportPng16`; the options set `jpegQuality`, `keepMetadata`, `removeGps`, `exportResize`, `exportLongEdge`, `exportOverwrite`, which `exportImage` and `startExportBatch` read.

## Window shell (`shell/`)

### `shell/shortcuts.hpp`
Every keyboard shortcut. LiteUI's `addShortcut` is **global**: it fires whatever has focus, so these work after clicking a sidebar button too. Modifiers must match exactly. Zoom uses the unshifted Ctrl+= / Ctrl+-: on Linux LiteUI reports shifted symbols like `+` as `Key::Unknown`, so Ctrl+Shift+= can't be bound there (it is bound as well, for layouts that report it). Three small wrappers make the table below safe. **`idle(fn)`**: ignored mid-drag (a slider or crop handle is held; changing the image under the drag would fight it). **`notTyping(fn)`**: also ignored while a name field has focus (`typingInTextField()`), so plain keys don't fire while typing. **`fieldFirst(key, fn)`**: Left, Right, Delete and Enter are also the keys a text field edits with; while one has focus it gets the key (`forwardKeyToTextField`), otherwise the shortcut runs. Called once from `main()`.

**Depends on:** `canvas_view.hpp`, `crop.hpp`, `crop_tool.hpp`, `rotate.hpp`, `sync.hpp`, `export.hpp`, `filmstrip.hpp`, `meta_commands.hpp`, `widgets.hpp`, `masks.hpp`, `presets.hpp`, `virtual_copies.hpp`, `compare.hpp`, `wb_picker.hpp`, `auto_tone_hist.hpp`; engine: `geometry.hpp`, `photo_meta.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**The shortcuts** (every one is installed by `installShortcuts`; `Ctrl` is the control key on every platform)

| Keys | Action | Guard |
|---|---|---|
| Ctrl+Z | `undo` | idle |
| Ctrl+Y, Ctrl+Shift+Z | `redo` | idle |
| Ctrl+O | `openImage` | idle |
| Ctrl+Shift+O | `openFolder` | idle |
| Ctrl+E | `exportImage` | idle |
| Ctrl+Shift+E | `exportSelected` | not typing, idle |
| Ctrl+Shift+S | `syncSettings` | not typing, idle |
| Ctrl+A | `filmSelectAll` | not typing |
| Ctrl+Shift+C / Ctrl+Shift+V | `copySettings` / `pasteSettings` | not typing (paste also idle) |
| Ctrl+Shift+N | `createVirtualCopy` | not typing, idle |
| Ctrl+Q | closes the window | none |
| Ctrl+B | `toggleSidebar` | none |
| Ctrl+= (and Ctrl+Shift+=), Ctrl+- , Ctrl+0 | zoom in, zoom out, 100% (`setZoom`) | none |
| Ctrl+[ , Ctrl+] | rotate left / right (`applyGeoOp`) | idle |
| Left, Right | `filmStep(−1)` / `filmStep(+1)` | field first, not typing |
| F6 | `toggleFilmstrip` | none |
| \\ | `toggleBefore` | not typing |
| Y | `toggleSplit` | not typing |
| J, Shift+J | `toggleClipping`, `toggleChannelClipping` | not typing |
| W | `toggleWbPicker` | not typing |
| A | `autoTone` | not typing |
| 0 .. 5 | `ratePhoto(0..5)` (0 clears) | not typing |
| P, X, U | `flagPhoto` Pick, Reject, None | not typing |
| 6, 7, 8, 9 | `labelPhoto(1..4)` (the first four colours) | not typing |
| Escape | close the open menu; cancel the white-balance picker | none |
| Delete | `deleteSelectedMask` while the Masks pane is open | field first, not typing |
| Enter | `applyCrop` while the Crop pane is open | field first, idle |

**Functions**

**`static void installShortcuts(LiteUI &ui)`**: **Input:** the `LiteUI` to register on. Registers every shortcut in the table above, in one place. Nothing here is a pane's own business: each entry just calls the same function the matching menu item or button calls.

### `shell/titlebar.hpp`
The title bar: the app title, five dropdown menus (**File**, **View**, **Image**, **Photo**), the undo / redo icon buttons and the three window-control buttons. Empty space in it drags the window (`ui.requestMove()`) and closes any open dropdown. The `ui` parameter is the `LiteUI` that owns the window; the click handlers capture it by reference, which is safe because `ui` outlives the view tree.

**Depends on:** `canvas_view.hpp`, `rotate.hpp`, `sync.hpp`, `export.hpp`, `filmstrip.hpp`, `meta_commands.hpp`, `widgets.hpp`, `presets.hpp`, `virtual_copies.hpp`, `compare.hpp`, `auto_tone_hist.hpp`; engine: `geometry.hpp`, `photo_meta.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**The menus**

| Menu | Items (shortcut) |
|---|---|
| File | Open Image... (Ctrl+O), Open Folder... (Ctrl+Shift+O), Export... (Ctrl+E), Export Selected... (Ctrl+Shift+E), Export All Shown..., Stop Export, Exit (Ctrl+Q) |
| View | Zoom In (Ctrl+=), Zoom Out (Ctrl+-), Actual Size (100%) (Ctrl+0), Toggle Sidebar (Ctrl+B), Toggle Filmstrip (F6), Previous Image (Left), Next Image (Right), Before / After (\\), Before \| After Split (Y), Clipping Warnings (J), Clipping: Per-Channel Colors (Shift+J) |
| Image | Rotate Left 90° (Ctrl+[), Rotate Right 90° (Ctrl+]), Flip Horizontal, Flip Vertical, Auto Tone (A), Copy Settings (Ctrl+Shift+C), Paste Settings (Ctrl+Shift+V), Select All Photos (Ctrl+A), Sync Settings to Selected (Ctrl+Shift+S), Create Virtual Copy (Ctrl+Shift+N), Delete Virtual Copy |
| Photo | Rate ★ .. ★★★★★ (1-5), Clear Rating (0), Flag as Pick (P), Flag as Rejected (X), Remove Flag (U), Label <colour> (6-9 for the first four), Clear Label |

After the menus: the **undo** (↶) and **redo** (↷) buttons, dimmed when `adjust.canUndo()` / `canRedo()` is false; a spacer; then minimize (─), maximize (□, which toggles `state.maximized`) and close (×).

**Functions**

**`static View buildTitlebar(LiteUI &ui)`**: **Input:** the `LiteUI`. **Returns** the title bar `View`. Uses `menuButton`, `titlebarIconBtn` and `winButton` (`widgets.hpp`).

### `shell/activity_bar.hpp`
The activity bar, the icon column at the far left. One 32 px button per entry of the `activities` table that `main()` builds (id, name, glyph): Info (`I`), History (`H`), Snapshots (`S`), Adjust (`A`), Presets (`P`), LUTs (`L`), Curve (`~`), Color Mix (`M`), Color Grading (`G`), Masks (◐), Crop (`C`), Rotate (`R`), Export (`E`). A button is highlighted while its pane is open; clicking it opens that pane, or closes the sidebar if it is already the open one (`state.activeActivity`, `-1` = closed). Opening Color Grading widens the sidebar to `kGradePaneW` if it is narrower, because the wheels are laid out for a minimum width.

**Depends on:** `color_ui.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildActivityBar(const std::vector<Activity> &activities)`**: **Input:** the activity table. **Returns** the activity bar.

### `shell/histogram_view.hpp`
The histogram canvas pinned above every pane, like Lightroom. It shows the histogram of the picture as currently shown (`computeHistogram`); a click cycles it through the histogram, a waveform and a vectorscope (`cycleScope`). The top-left and top-right corners are the clipping toggles (shadows, highlights). Its height follows the view (`kHistHeightPx` or `kScopeHeightPx`). Its dirty source calls `refreshHistogramIfStale()` first (the sidebar is polled before the main canvas, so this does the pending recompute) and repaints only when `histVersion` changed or the active pane changed.

**Depends on:** `widgets.hpp`, `auto_tone_hist.hpp`; engine: `scopes.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildHistogramView()`**: **Returns** the histogram canvas. The pointer handler stops propagation and closes menus; with a picture open, a press in the top-left corner (within `kHistTriHitPx`) toggles the shadow overlay, in the top-right corner the highlight overlay (only once the canvas has been painted, `uiState.hist.canvasW > 0`), anywhere else cycles the view.

### `shell/sidebar.hpp`
The left sidebar: a title that follows the open pane, the pinned histogram, then every pane in the order of the activity bar, in a scrolling body. Panes are built once and are always in the tree; each shows itself only while its activity is selected (the sidebar's own width follows `state.activeActivity`, 0 when closed). The title is the activity's name. `maskEditor` is an **out-parameter**: the Masks pane builds the selected mask's editor together with itself, but the editor lives in the right-hand panel, so `buildSidebar` hands it back for `addMaskPanel`. The `activities` table is captured by value by the title's label lambda.

**Depends on:** `panes/info_pane.hpp`, `panes/history_pane.hpp`, `panes/snapshots_pane.hpp`, `panes/adjust_pane.hpp`, `panes/presets_pane.hpp`, `panes/luts_pane.hpp`, `panes/curve_pane.hpp`, `panes/mix_pane.hpp`, `panes/grade_pane.hpp`, `panes/masks_pane.hpp`, `panes/crop_pane.hpp`, `panes/rotate_pane.hpp`, `panes/export_pane.hpp`, `shell/histogram_view.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildSidebar(const std::vector<Activity> &activities, View &maskEditor)`**: **Inputs:** the activity table and an out-reference for the mask editor. **Returns** the sidebar shell: title, histogram, then the 13 panes in order (Info, History, Snapshots, Adjust, Presets, LUTs, Curve, Color Mix, Color Grade, Masks, Crop, Rotate, Export).

### `shell/sidebar_divider.hpp`
The vertical divider to the right of the sidebar. Dragging it resizes the sidebar (`state.sidebarWidth`, clamped to `kMinSidebarW..kMaxSidebarW`); the cursor changes to the horizontal-resize one.

**Depends on:** `widgets.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildSidebarDivider()`**: **Returns** the divider. `bindDrag` with the same `resize` for press and drag and the horizontal-resize cursor.

### `shell/center_column.hpp`
The middle of the window: the **canvas** on top and the **filmstrip** underneath, so the filmstrip stays under the picture only and the sidebar keeps its full height. The canvas is as big as the area under the toolbar whatever the zoom; `paintCanvas` draws the picture into it, centred or scrolled (see `ViewGeo`). It only occupies the slot while an image is open; the "No image selected" placeholder is the other child for that same slot. The canvas owns all pointer logic for the picture: scrollbars, panning, compare-split drag, the white-balance picker, crop, masks, and right-clicks. It also does the **per-frame poll**: its dirty source runs `flushPendingAdjust()` and then consumes `state.dirty`.

**Depends on:** `canvas_view.hpp`, `crop.hpp`, `crop_tool.hpp`, `filmstrip.hpp`, `widgets.hpp`, `masks.hpp`, `virtual_copies.hpp`, `mask_list.hpp`, `mask_polygon.hpp`, `mask_brush.hpp`, `compare.hpp`, `canvas_paint.hpp`, `readout.hpp`, `wb_picker.hpp`, `auto_tone_hist.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildCenterColumn()`**: **Returns** the centre column. The **canvas** records its size on every layout (`state.view.w/h`, then `viewGeo()` to keep the scroll in range) and handles the pointer. A **press** closes menus and stops propagation, then in this order: a scrollbar press (`viewBarPress`); a middle-button press starts a pan; a right-click with no compare mode goes to `objectPress` and `polygonRightPress`; a left press records the owning pointer and, in a compare mode, moves the split line (`splitFromX`); with no tool active (no picker, no Crop pane, no Masks pane) a left press pans; otherwise the white-balance picker (`wbPress`) gets it first, then `cropPress` and `maskPress` (each ignores the press unless its own pane is open). A gesture belongs to **one pointer** (`pressed`, `viewPointer`), as in `bindDrag`, so a second finger is ignored. **Moves** continue the view drag, update the RGB readout (`hoverSample`), and continue a compare, crop, mask or picker drag; **up** and **cancel** end them. The cursor changes for the split line (sideways) and for the tools and the picker. The **wheel** zooms about the pointer (`zoomAbout`): a notched wheel steps by `kZoomStep`, a trackpad (precise pixel deltas) zooms continuously. The **placeholder** ("No image selected") fills the canvas slot while no image is open. The **filmstrip** canvas repaints from its own dirty poll, a press goes to `filmClick`, and the wheel scrolls it (two cells per notch; a trackpad scrolls by its pixel delta and honours sideways swipes).

### `shell/mask_panel.hpp`
The right-hand panel with the selected mask's controls, plus its divider. Only shown on the Masks pane with a mask selected (`maskToolActive() && selMask() >= 0`), so every other pane keeps the full canvas width. Dragging its divider resizes it (`state.maskPanelW`, clamped to `kMinMaskPanelW..kMaxMaskPanelW`; the panel sits to the right, so it grows as the divider moves left). It has a title ("MASK N  <name>") and a scrolling body holding `maskEditor`, the view the Masks pane built. Adds **both** the divider and the panel to `workspace`.

**Depends on:** `widgets.hpp`, `masks.hpp`, `mask_widgets.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static void addMaskPanel(View &workspace, View maskEditor)`**: **Inputs:** the workspace to add to and the mask editor view (moved in). Builds the divider (`rDivider`) and the panel and appends both to `workspace`, in that order.

### `shell/status_bar.hpp`
The status bar at the bottom: on the left one line joining the picture's name and size ("No image open" without one), a hint while Before or Before / After is on, and the status message, separated by bullets; and on the right the RGB readout under the cursor (a colour swatch and `R G B` values in a fixed-width box so the zoom controls never shift as the numbers change) and the zoom controls (zoom out, the percentage, zoom in, 100%). Text colours follow `state.statusIsError`.

**Depends on:** `canvas_view.hpp`, `saved_edits.hpp`. **Test:** no case of its own in `tests/cases/` (those cover the engine headers); it is compiled into `golden_render_test` and `lut_pane_test`, which include `main.cpp`.

**Functions**

**`static View buildStatusBar()`**: **Returns** the status bar. The left text is `<picture>  —  W x H px` (or "No image open"), then "Before (press \\ to return)" or "Before | After (Y to close, drag to move the line)" while a compare mode is on, then the status message, joined with bullets; it is red while `state.statusIsError`, and never wraps. The zoom label shows `zoomDev` as a percentage.

## Include order

`main.cpp` includes these in exactly this order, right after `startup_checks.hpp` for the `panes/` and `shell/` ones. The order is part of the program: later files use earlier ones, a few functions are forward-declared, and static variables are initialised in definition order.

| # | File | What it holds |
|---|---|---|
| 1 | [`constants.hpp`](#constantshpp) | layout constants and the color palette |
| 2 | [`render_worker.hpp`](#render_workerhpp) | the background render worker (fence: renderworker) |
| 3 | [`editor_state.hpp`](#editor_statehpp) | crop / filmstrip / compare state and EditorState (the global `state`) |
| 4 | [`ui_state.hpp`](#ui_statehpp) | UI-only state: tabs, selections, clipboards (`uiState`) |
| 5 | [`canvas_view.hpp`](#canvas_viewhpp) | where the picture sits inside the canvas: zoom, scroll, scrollbars |
| 6 | [`render_update.hpp`](#render_updatehpp) | handing the render worker's result back, drag preview, image refresh |
| 7 | [`crop.hpp`](#crophpp) | crop selection helpers |
| 8 | [`geometry_worker.hpp`](#geometry_workerhpp) | the background geometry worker and its UI-thread glue |
| 9 | [`crop_tool.hpp`](#crop_toolhpp) | crop tool: hit-testing, dragging, apply / remove crop |
| 10 | [`rotate.hpp`](#rotatehpp) | rotate / flip and straighten |
| 11 | [`decode.hpp`](#decodehpp) | opening a file: the background decode worker |
| 12 | [`segmentation.hpp`](#segmentationhpp) | object segmentation worker and engine info |
| 13 | [`saved_edits.hpp`](#saved_editshpp) | saved edits: the sidecar file |
| 14 | [`lut_pane.hpp`](#lut_panehpp) | 3D LUT list, previews and cache |
| 15 | [`sync.hpp`](#synchpp) | Sync Settings: one look onto other pictures |
| 16 | [`export.hpp`](#exporthpp) | batch export from the filmstrip |
| 17 | [`filmstrip.hpp`](#filmstriphpp) | the filmstrip (fence: filmstrip) |
| 18 | [`meta_commands.hpp`](#meta_commandshpp) | rating, flag and color label commands |
| 19 | [`widgets.hpp`](#widgetshpp) | small reusable widgets, pointer helpers, text-field keys |
| 20 | [`color_ui.hpp`](#color_uihpp) | Color Mixer and Color Grading UI |
| 21 | [`masks.hpp`](#maskshpp) | masks: selection, editing, overlay color, name field |
| 22 | [`presets.hpp`](#presetshpp) | presets and Copy / Paste Settings |
| 23 | [`snapshots.hpp`](#snapshotshpp) | named snapshots |
| 24 | [`virtual_copies.hpp`](#virtual_copieshpp) | virtual copies |
| 25 | [`mask_list.hpp`](#mask_listhpp) | mask list operations and parts |
| 26 | [`mask_shapes.hpp`](#mask_shapeshpp) | radial, linear and rectangle mask geometry on the canvas |
| 27 | [`mask_polygon.hpp`](#mask_polygonhpp) | polygon / lasso tool |
| 28 | [`mask_brush.hpp`](#mask_brushhpp) | brush painting |
| 29 | [`mask_overlays.hpp`](#mask_overlayshpp) | brush, luminosity and combined mask overlays |
| 30 | [`mask_widgets.hpp`](#mask_widgetshpp) | Masks pane widgets (also the preset / LUT / snapshot rows) |
| 31 | [`history_panel.hpp`](#history_panelhpp) | the History panel |
| 32 | [`crop_overlay.hpp`](#crop_overlayhpp) | paints the crop selection over the picture |
| 33 | [`compare.hpp`](#comparehpp) | Before / After (fence: compare) |
| 34 | [`canvas_paint.hpp`](#canvas_painthpp) | clipping overlay, GPU copies of the picture, paintCanvas |
| 35 | [`readout.hpp`](#readouthpp) | RGB readout under the cursor |
| 36 | [`wb_picker.hpp`](#wb_pickerhpp) | white-balance picker |
| 37 | [`auto_tone_hist.hpp`](#auto_tone_histhpp) | Auto Tone, flushPendingAdjust, histogram and scopes |
| 38 | [`curve_editor.hpp`](#curve_editorhpp) | tone curve editor (canvas side) |
| 39 | [`mask_effects.hpp`](#mask_effectshpp) | mask effects: Amount, Color Mix, Grading, Curve |
| 40 | [`mask_curve.hpp`](#mask_curvehpp) | mask tone curve editor (canvas side) |
| 41 | [`startup_checks.hpp`](#startup_checkshpp) | verifyAdjustmentSpecs (runs once at startup) |
| 42 | [`panes/pane_helpers.hpp`](#panespane_helpershpp) | small views shared by several panes |
| 43 | [`panes/info_pane.hpp`](#panesinfo_panehpp) | the Info pane (buildInfoPane) |
| 44 | [`panes/history_pane.hpp`](#paneshistory_panehpp) | the History pane (buildHistoryPane) |
| 45 | [`panes/snapshots_pane.hpp`](#panessnapshots_panehpp) | the Snapshots pane (buildSnapshotsPane) |
| 46 | [`panes/adjust_pane.hpp`](#panesadjust_panehpp) | the Adjust pane (buildAdjustPane) |
| 47 | [`panes/presets_pane.hpp`](#panespresets_panehpp) | the Presets pane (buildPresetsPane) |
| 48 | [`panes/luts_pane.hpp`](#panesluts_panehpp) | the LUTs pane (buildLutsPane) |
| 49 | [`panes/curve_pane.hpp`](#panescurve_panehpp) | the Curve pane (buildCurvePane) |
| 50 | [`panes/mix_pane.hpp`](#panesmix_panehpp) | the Color Mix pane (buildMixPane) |
| 51 | [`panes/grade_pane.hpp`](#panesgrade_panehpp) | the Color Grade pane (buildGradePane) |
| 52 | [`panes/masks_pane.hpp`](#panesmasks_panehpp) | the Masks pane and its right-hand editor (buildMaskPanes) |
| 53 | [`panes/crop_pane.hpp`](#panescrop_panehpp) | the Crop pane (buildCropPane) |
| 54 | [`panes/rotate_pane.hpp`](#panesrotate_panehpp) | the Rotate pane (buildRotatePane) |
| 55 | [`panes/export_pane.hpp`](#panesexport_panehpp) | the Export pane (buildExportPane) |
| 56 | [`shell/shortcuts.hpp`](#shellshortcutshpp) | keyboard shortcuts (installShortcuts) |
| 57 | [`shell/titlebar.hpp`](#shelltitlebarhpp) | the title bar (buildTitlebar) |
| 58 | [`shell/activity_bar.hpp`](#shellactivity_barhpp) | the activity bar (buildActivityBar) |
| 59 | [`shell/histogram_view.hpp`](#shellhistogram_viewhpp) | the pinned histogram / scopes view (buildHistogramView) |
| 60 | [`shell/sidebar.hpp`](#shellsidebarhpp) | the left sidebar: title, histogram and every pane (buildSidebar) |
| 61 | [`shell/sidebar_divider.hpp`](#shellsidebar_dividerhpp) | the draggable divider right of the sidebar (buildSidebarDivider) |
| 62 | [`shell/center_column.hpp`](#shellcenter_columnhpp) | canvas, "no image" placeholder and filmstrip (buildCenterColumn) |
| 63 | [`shell/mask_panel.hpp`](#shellmask_panelhpp) | the right-hand mask panel and its divider (addMaskPanel) |
| 64 | [`shell/status_bar.hpp`](#shellstatus_barhpp) | the status bar (buildStatusBar) |

## Global variables and tables

Every file-scope variable defined under `src/app/` (constants are listed in each entry). All are UI-thread only unless the entry says otherwise.

| Variable | File | Meaning |
|---|---|---|
| `renderWorker` | [`render_worker.hpp`](#render_workerhpp) |  |
| `kCropAspects` | [`editor_state.hpp`](#editor_statehpp) |  |
| `state` | [`editor_state.hpp`](#editor_statehpp) |  |
| `uiState` | [`ui_state.hpp`](#ui_statehpp) |  |
| `geometryWorker` | [`geometry_worker.hpp`](#geometry_workerhpp) |  |
| `decodeWorker` | [`decode.hpp`](#decodehpp) |  |
| `gSegInfoMutex` | [`segmentation.hpp`](#segmentationhpp) | seg_basic.hpp as fallback) ---- Which engine is active, shown in the Object panel (written by the worker, read by the UI thread's label poll). |
| `gSegInfo` | [`segmentation.hpp`](#segmentationhpp) |  |
| `segmentWorker` | [`segmentation.hpp`](#segmentationhpp) |  |
| `gLuts` | [`lut_pane.hpp`](#lut_panehpp) | The rows of the LUTs list, at most `kLutRows`. |
| `gLutsLoaded` | [`lut_pane.hpp`](#lut_panehpp) | The folder was listed since the pane was last opened. |
| `gLutPaintCounter` | [`lut_pane.hpp`](#lut_panehpp) | Counter stamping each row's `paintVersion` whenever its preview or text changes. |
| `lutPreviewWorker` | [`lut_pane.hpp`](#lut_panehpp) | The worker that renders the previews (`LutPreviewWorker`, `workers/lut_preview_worker.hpp`). |
| `gLutSerial` | [`lut_pane.hpp`](#lut_panehpp) | The serial of the batch whose results are taken in; results of an earlier batch are ignored. |
| `gLutBuilt` | [`lut_pane.hpp`](#lut_panehpp) | What the current previews were made for. |
| `gLutCacheEpoch` | [`lut_pane.hpp`](#lut_panehpp) | Bumped when the LUT cache is emptied; `currentLutStatus()` looks up again when it changes. |
| `exportWorker` | [`export.hpp`](#exporthpp) | The worker that exports the batch (`ExportWorker`, `workers/export_worker.hpp`). |
| `exportRun` | [`export.hpp`](#exporthpp) | The running batch's progress. |
| `thumbWorker` | [`filmstrip.hpp`](#filmstriphpp) | The worker that makes the thumbnails (`ThumbWorker`, `workers/thumb_worker.hpp`). |
| `kLabelColors` | [`filmstrip.hpp`](#filmstriphpp) | The five colour labels' colours (Red, Yellow, Green, Blue, Purple). |
| `gTextFields` | [`widgets.hpp`](#widgetshpp) | Every registered name field (see `registerTextField`); only ever appended to. |
| `kMaskTintPalette` | [`masks.hpp`](#maskshpp) | The colours the selected mask's overlay can be painted in (Red, Green, Blue, Yellow, ...). One place, so every overlay (radial / linear gradients, brush, luminosity, colour range and combined bitmaps) agrees. |
| `kMaskTintCount` | [`masks.hpp`](#maskshpp) | Number of palette entries. |
| `gImageTex` | [`canvas_paint.hpp`](#canvas_painthpp) | The three picture-texture slots: the edited image, the original (`source`, for Before and Split) and the mid-drag preview. Mutated during paint, on the UI thread (not part of `uiState`, which holds no GPU objects). |
| `gSourceTex` | [`canvas_paint.hpp`](#canvas_painthpp) | (see `gImageTex`) |
| `gPreviewTex` | [`canvas_paint.hpp`](#canvas_painthpp) | (see `gImageTex`) |

## Function index

**A** `addMask` ([virtual_copies](#virtual_copieshpp)), `addMaskPanel` ([shell/mask_panel](#shellmask_panelhpp)), `addPart` ([mask_list](#mask_listhpp)), `addSnapshot` ([snapshots](#snapshotshpp)), `adoptDecoded` ([meta_commands](#meta_commandshpp)), `adoptFullRender` ([render_update](#render_updatehpp)), `adoptGeometryBundle` ([geometry_worker](#geometry_workerhpp)), `adoptSegment` ([segmentation](#segmentationhpp)), `announceColorTag` ([saved_edits](#saved_editshpp)), `applyClickCursors` ([widgets](#widgetshpp)), `applyCrop` ([crop_tool](#crop_toolhpp)), `applyGeoOp` ([rotate](#rotatehpp)), `applyLutRow` ([lut_pane](#lut_panehpp)), `applyMetaPatch` ([meta_commands](#meta_commandshpp)), `applyPresetEntry` ([presets](#presetshpp)), `applySnapshotEntry` ([snapshots](#snapshotshpp)), `applyZoomDev` ([editor_state](#editor_statehpp)), `autosaveSidecar` ([saved_edits](#saved_editshpp)), `autoTone` ([auto_tone_hist](#auto_tone_histhpp))  
**B** `bindDrag` ([widgets](#widgetshpp)), `brushBegin` ([mask_brush](#mask_brushhpp)), `brushDragTo` ([mask_brush](#mask_brushhpp)), `brushInView` ([masks](#maskshpp)), `brushPinHit` ([mask_brush](#mask_brushhpp)), `brushTintImage` ([mask_overlays](#mask_overlayshpp)), `buildActivityBar` ([shell/activity_bar](#shellactivity_barhpp)), `buildAdjustmentSlider` ([widgets](#widgetshpp)), `buildAdjustPane` ([panes/adjust_pane](#panesadjust_panehpp)), `buildCenterColumn` ([shell/center_column](#shellcenter_columnhpp)), `buildCropPane` ([panes/crop_pane](#panescrop_panehpp)), `buildCurvePane` ([panes/curve_pane](#panescurve_panehpp)), `buildExportPane` ([panes/export_pane](#panesexport_panehpp)), `buildGradeEye` ([color_ui](#color_uihpp)), `buildGradePane` ([panes/grade_pane](#panesgrade_panehpp)), `buildGradeSingle` ([color_ui](#color_uihpp)), `buildGradeThreeWay` ([color_ui](#color_uihpp)), `buildGradeWheel` ([color_ui](#color_uihpp)), `buildGradeWheelBlock` ([color_ui](#color_uihpp)), `buildGradeWheelSlider` ([color_ui](#color_uihpp)), `buildHistogramView` ([shell/histogram_view](#shellhistogram_viewhpp)), `buildHistoryPane` ([panes/history_pane](#paneshistory_panehpp)), `buildHistoryRow` ([history_panel](#history_panelhpp)), `buildInfoPane` ([panes/info_pane](#panesinfo_panehpp)), `buildLutRow` ([mask_widgets](#mask_widgetshpp)), `buildLutsPane` ([panes/luts_pane](#panesluts_panehpp)), `buildMaskAdjustmentSlider` ([history_panel](#history_panelhpp)), `buildMaskAmountSlider` ([mask_effects](#mask_effectshpp)), `buildMaskCurveCanvas` ([mask_curve](#mask_curvehpp)), `buildMaskGradeShared` ([mask_effects](#mask_effectshpp)), `buildMaskGradeSlider` ([mask_effects](#mask_effectshpp)), `buildMaskMixSlider` ([mask_effects](#mask_effectshpp)), `buildMaskPanes` ([panes/masks_pane](#panesmasks_panehpp)), `buildMaskRow` ([mask_widgets](#mask_widgetshpp)), `buildMixPane` ([panes/mix_pane](#panesmix_panehpp)), `buildMixSlider` ([color_ui](#color_uihpp)), `buildPartRow` ([history_panel](#history_panelhpp)), `buildPresetRow` ([mask_widgets](#mask_widgetshpp)), `buildPresetsPane` ([panes/presets_pane](#panespresets_panehpp)), `buildRotatePane` ([panes/rotate_pane](#panesrotate_panehpp)), `buildSidebar` ([shell/sidebar](#shellsidebarhpp)), `buildSidebarDivider` ([shell/sidebar_divider](#shellsidebar_dividerhpp)), `buildSliderRow` ([widgets](#widgetshpp)), `buildSnapshotRow` ([mask_widgets](#mask_widgetshpp)), `buildSnapshotsPane` ([panes/snapshots_pane](#panessnapshots_panehpp)), `buildStatusBar` ([shell/status_bar](#shellstatus_barhpp)), `buildTitlebar` ([shell/titlebar](#shelltitlebarhpp))  
**C** `changeView` ([canvas_view](#canvas_viewhpp)), `clearSelectedObject` ([virtual_copies](#virtual_copieshpp)), `clearStatus` ([editor_state](#editor_statehpp)), `closeMenus` ([widgets](#widgetshpp)), `collectDecodeJob` ([meta_commands](#meta_commandshpp)), `collectExportJob` ([export](#exporthpp)), `collectFullRender` ([render_update](#render_updatehpp)), `collectGeometryJob` ([geometry_worker](#geometry_workerhpp)), `collectSegmentJob` ([segmentation](#segmentationhpp)), `collectSyncJob` ([sync](#synchpp)), `collectThumbJobs` ([filmstrip](#filmstriphpp)), `colorRangePress` ([mask_brush](#mask_brushhpp)), `compositeTintImage` ([mask_overlays](#mask_overlayshpp)), `computeHistogram` ([auto_tone_hist](#auto_tone_histhpp)), `copyMaskEffects` ([mask_list](#mask_listhpp)), `copySettings` ([presets](#presetshpp)), `createVirtualCopy` ([virtual_copies](#virtual_copieshpp)), `cropAspect` ([crop](#crophpp)), `cropDrag` ([crop_tool](#crop_toolhpp)), `cropDragCorner` ([crop_tool](#crop_toolhpp)), `cropFitToAspect` ([crop](#crophpp)), `cropHitTest` ([crop_tool](#crop_toolhpp)), `cropPress` ([crop_tool](#crop_toolhpp)), `cropResize` ([crop_tool](#crop_toolhpp)), `cropToolActive` ([crop](#crophpp)), `currentColorTag` ([saved_edits](#saved_editshpp)), `currentLutStatus` ([lut_pane](#lut_panehpp)), `currentPhotoInfo` ([saved_edits](#saved_editshpp)), `curveActiveCurve` ([curve_editor](#curve_editorhpp)), `curveChannelColor` ([curve_editor](#curve_editorhpp)), `curveChannelName` ([curve_editor](#curve_editorhpp)), `curveDrag` ([curve_editor](#curve_editorhpp)), `curveEnd` ([curve_editor](#curve_editorhpp)), `curveFromLocal` ([curve_editor](#curve_editorhpp)), `curveHistogram` ([curve_editor](#curve_editorhpp)), `curveHit` ([curve_editor](#curve_editorhpp)), `curvePlotPx` ([curve_editor](#curve_editorhpp)), `curvePress` ([curve_editor](#curve_editorhpp)), `curveRightPress` ([curve_editor](#curve_editorhpp)), `curveToLocal` ([curve_editor](#curve_editorhpp)), `cycleScope` ([auto_tone_hist](#auto_tone_histhpp))  
**D** `deleteSelectedMask` ([virtual_copies](#virtual_copieshpp)), `deleteSelectedPreset` ([presets](#presetshpp)), `deleteSelectedSnapshot` ([snapshots](#snapshotshpp)), `deleteVirtualCopy` ([virtual_copies](#virtual_copieshpp)), `dropLutCache` ([lut_pane](#lut_panehpp)), `duplicateSelectedMask` ([mask_list](#mask_listhpp))  
**E** `editLinearInView` ([masks](#maskshpp)), `editRadialInView` ([masks](#maskshpp)), `editShapeInView` ([masks](#maskshpp)), `enterDrag` ([render_update](#render_updatehpp)), `exportAllShown` ([export](#exporthpp)), `exportBusy` ([export](#exporthpp)), `exportImage` ([meta_commands](#meta_commandshpp)), `exportSelected` ([export](#exporthpp)), `exportTargets` ([export](#exporthpp))  
**F** `filmAdoptFiles` ([filmstrip](#filmstriphpp)), `filmAdoptFolder` ([filmstrip](#filmstriphpp)), `filmApplyMeta` ([filmstrip](#filmstriphpp)), `filmBarClick` ([filmstrip](#filmstriphpp)), `filmBarItems` ([filmstrip](#filmstriphpp)), `filmClampScroll` ([filmstrip](#filmstriphpp)), `filmClearSelection` ([filmstrip](#filmstriphpp)), `filmClick` ([filmstrip](#filmstriphpp)), `filmEnsureVisible` ([filmstrip](#filmstriphpp)), `filmFilterChanged` ([filmstrip](#filmstriphpp)), `filmInsertEntry` ([virtual_copies](#virtual_copieshpp)), `filmLeave` ([filmstrip](#filmstriphpp)), `filmListFolder` ([filmstrip](#filmstriphpp)), `filmMarkEdited` ([filmstrip](#filmstriphpp)), `filmOpenIndex` ([filmstrip](#filmstriphpp)), `filmPushEntry` ([filmstrip](#filmstriphpp)), `filmPushWithCopies` ([filmstrip](#filmstriphpp)), `filmQueueThumbs` ([filmstrip](#filmstriphpp)), `filmRebuildView` ([filmstrip](#filmstriphpp)), `filmRemoveEntry` ([virtual_copies](#virtual_copieshpp)), `filmScrollBy` ([filmstrip](#filmstriphpp)), `filmSelectAll` ([filmstrip](#filmstriphpp)), `filmStep` ([filmstrip](#filmstriphpp)), `filmstripShown` ([filmstrip](#filmstriphpp)), `filmSync` ([filmstrip](#filmstriphpp)), `filmThumbTexture` ([filmstrip](#filmstriphpp)), `filmViewPos` ([filmstrip](#filmstriphpp)), `filterPresetNameField` ([presets](#presetshpp)), `filterSnapshotNameField` ([snapshots](#snapshotshpp)), `finishFullRender` ([render_update](#render_updatehpp)), `finishGeometryJob` ([geometry_worker](#geometry_workerhpp)), `flagPhoto` ([meta_commands](#meta_commandshpp)), `flushPendingAdjust` ([auto_tone_hist](#auto_tone_histhpp)), `flushSidecar` ([saved_edits](#saved_editshpp)), `forwardKeyToTextField` ([widgets](#widgetshpp))  
**G** `geometryBusy` ([geometry_worker](#geometry_workerhpp)), `geometryDirty` ([geometry_worker](#geometry_workerhpp)), `getSegInfo` ([segmentation](#segmentationhpp)), `gradeEdit` ([color_ui](#color_uihpp)), `gradeEndDrag` ([color_ui](#color_uihpp)), `gradeHash` ([color_ui](#color_uihpp)), `gradeHsv` ([color_ui](#color_uihpp)), `gradeMix` ([color_ui](#color_uihpp)), `gradeRangeName` ([color_ui](#color_uihpp)), `gradeWheelImage` ([color_ui](#color_uihpp))  
**H** `historyStepForRow` ([history_panel](#history_panelhpp)), `hoverClear` ([readout](#readouthpp)), `hoverSample` ([readout](#readouthpp))  
**I** `importLutPaths` ([lut_pane](#lut_panehpp)), `importLuts` ([lut_pane](#lut_panehpp)), `installShortcuts` ([shell/shortcuts](#shellshortcutshpp))  
**J** `jumpToHistory` ([history_panel](#history_panelhpp))  
**L** `labelPhoto` ([meta_commands](#meta_commandshpp)), `linearGeom` ([mask_shapes](#mask_shapeshpp)), `linearHit` ([mask_shapes](#mask_shapeshpp)), `linearInView` ([masks](#maskshpp)), `loadSidecar` ([lut_pane](#lut_panehpp)), `luminosityTintImage` ([mask_overlays](#mask_overlayshpp)), `lumTintBase` ([mask_overlays](#mask_overlayshpp)), `lutCurrentText` ([lut_pane](#lut_panehpp)), `lutPreviewsCurrent` ([lut_pane](#lut_panehpp)), `lutPreviewsFollow` ([lut_pane](#lut_panehpp)), `lutReason` ([lut_pane](#lut_panehpp)), `lutRowDetail` ([lut_pane](#lut_panehpp))  
**M** `makeTestObjectMap` ([virtual_copies](#virtual_copieshpp)), `maskChipW` ([masks](#maskshpp)), `maskCurveCommit` ([mask_curve](#mask_curvehpp)), `maskCurveCopy` ([mask_curve](#mask_curvehpp)), `maskCurveDrag` ([mask_curve](#mask_curvehpp)), `maskCurveEnd` ([mask_curve](#mask_curvehpp)), `maskCurvePress` ([mask_curve](#mask_curvehpp)), `maskCurveRightPress` ([mask_curve](#mask_curvehpp)), `maskDrag` ([mask_brush](#mask_brushhpp)), `maskEdit` ([masks](#maskshpp)), `maskEditDiscrete` ([masks](#maskshpp)), `maskEndDrag` ([mask_list](#mask_listhpp)), `maskHit` ([mask_shapes](#mask_shapeshpp)), `maskIntText` ([mask_effects](#mask_effectshpp)), `maskLabel` ([mask_widgets](#mask_widgetshpp)), `maskLinearSlider` ([history_panel](#history_panelhpp)), `maskPress` ([mask_brush](#mask_brushhpp)), `maskRadialSlider` ([history_panel](#history_panelhpp)), `maskShapeSlider` ([history_panel](#history_panelhpp)), `maskSignedText` ([mask_effects](#mask_effectshpp)), `maskTint` ([masks](#maskshpp)), `maskTintColor` ([masks](#maskshpp)), `maskToolActive` ([masks](#maskshpp)), `mcFromLocal` ([mask_curve](#mask_curvehpp)), `mcHit` ([mask_curve](#mask_curvehpp)), `mcPlotPx` ([mask_curve](#mask_curvehpp)), `mcToLocal` ([mask_curve](#mask_curvehpp)), `menuButton` ([widgets](#widgetshpp)), `metaBatchTargets` ([meta_commands](#meta_commandshpp)), `mixBandColor` ([constants](#constantshpp)), `moveSelectedMask` ([mask_list](#mask_listhpp))  
**N** `newMaskShape` ([virtual_copies](#virtual_copieshpp)), `nextSnapshotName` ([snapshots](#snapshotshpp)), `noteLutProblem` ([lut_pane](#lut_panehpp)), `nudgeAngle` ([rotate](#rotatehpp))  
**O** `objectCurrentPrompts` ([virtual_copies](#virtual_copieshpp)), `objectPrefetch` ([virtual_copies](#virtual_copieshpp)), `objectPress` ([virtual_copies](#virtual_copieshpp)), `openFolder` ([filmstrip](#filmstriphpp)), `openImage` ([meta_commands](#meta_commandshpp))  
**P** `paintBrushCursor` ([mask_overlays](#mask_overlayshpp)), `paintBrushPin` ([mask_overlays](#mask_overlayshpp)), `paintBrushTint` ([mask_overlays](#mask_overlayshpp)), `paintCanvas` ([canvas_paint](#canvas_painthpp)), `paintClipOverlay` ([canvas_paint](#canvas_painthpp)), `paintClipTriangles` ([auto_tone_hist](#auto_tone_histhpp)), `paintCompareLabel` ([compare](#comparehpp)), `paintCompareOverlay` ([compare](#comparehpp)), `paintCompositeTint` ([mask_overlays](#mask_overlayshpp)), `paintCropOverlay` ([crop_overlay](#crop_overlayhpp)), `paintCurve` ([curve_editor](#curve_editorhpp)), `paintFilmstrip` ([filmstrip](#filmstriphpp)), `paintHistogram` ([auto_tone_hist](#auto_tone_histhpp)), `paintHistogramBars` ([auto_tone_hist](#auto_tone_histhpp)), `paintLinearOutline` ([mask_brush](#mask_brushhpp)), `paintLinearTint` ([mask_brush](#mask_brushhpp)), `paintLuminosityTint` ([mask_overlays](#mask_overlayshpp)), `paintMaskCurve` ([mask_curve](#mask_curvehpp)), `paintMaskOverlay` ([mask_overlays](#mask_overlayshpp)), `paintPartOutline` ([mask_overlays](#mask_overlayshpp)), `paintPolygonOutline` ([mask_brush](#mask_brushhpp)), `paintRadialOutline` ([mask_brush](#mask_brushhpp)), `paintRadialTint` ([mask_brush](#mask_brushhpp)), `paintRectOutline` ([mask_brush](#mask_brushhpp)), `paintScope` ([auto_tone_hist](#auto_tone_histhpp)), `paintSelectedObjectPrompts` ([mask_overlays](#mask_overlayshpp)), `paintViewBars` ([canvas_view](#canvas_viewhpp)), `pasteMaskEffects` ([mask_list](#mask_listhpp)), `pasteSettings` ([presets](#presetshpp)), `pictureLabel` ([saved_edits](#saved_editshpp)), `pictureTexture` ([canvas_paint](#canvas_painthpp)), `pollLutPreviews` ([lut_pane](#lut_panehpp)), `polyClampCoord` ([mask_polygon](#mask_polygonhpp)), `polyDocPts` ([mask_polygon](#mask_polygonhpp)), `polygonBegin` ([mask_polygon](#mask_polygonhpp)), `polygonBodyHit` ([mask_polygon](#mask_polygonhpp)), `polygonDirty` ([mask_polygon](#mask_polygonhpp)), `polygonDragTo` ([mask_polygon](#mask_polygonhpp)), `polygonEditPts` ([mask_polygon](#mask_polygonhpp)), `polygonInView` ([masks](#maskshpp)), `polygonRightPress` ([mask_polygon](#mask_polygonhpp)), `polyVertexAt` ([mask_polygon](#mask_polygonhpp)), `previewTexture` ([canvas_paint](#canvas_painthpp))  
**R** `radialGeom` ([mask_shapes](#mask_shapeshpp)), `radialInView` ([masks](#maskshpp)), `ratePhoto` ([meta_commands](#meta_commandshpp)), `readSidecarText` ([sync](#synchpp)), `rectGeom` ([mask_shapes](#mask_shapeshpp)), `rectHit` ([mask_shapes](#mask_shapeshpp)), `rectInView` ([masks](#maskshpp)), `redo` ([rotate](#rotatehpp)), `refreshHistogramIfStale` ([auto_tone_hist](#auto_tone_histhpp)), `refreshLuts` ([lut_pane](#lut_panehpp)), `refreshPresets` ([presets](#presetshpp)), `registerTextField` ([widgets](#widgetshpp)), `removeCrop` ([crop_tool](#crop_toolhpp)), `removeLut` ([lut_pane](#lut_panehpp)), `removeSelectedPart` ([mask_list](#mask_listhpp)), `renameSelectedMask` ([masks](#maskshpp)), `resetAllCurves` ([curve_editor](#curve_editorhpp)), `resetColorGrade` ([color_ui](#color_uihpp)), `resetColorMix` ([color_ui](#color_uihpp)), `resetCropSelection` ([crop](#crophpp)), `resetCurve` ([curve_editor](#curve_editorhpp)), `resetOrientation` ([rotate](#rotatehpp)), `runDecodeJob` ([decode](#decodehpp)), `runGeometryJob` ([geometry_worker](#geometry_workerhpp)), `runRenderJob` ([render_worker](#render_workerhpp)), `runSegmentJob` ([segmentation](#segmentationhpp)), `runSyncJob` ([sync](#synchpp))  
**S** `sameOwner` ([mask_overlays](#mask_overlayshpp)), `samModelDir` ([segmentation](#segmentationhpp)), `samplePreMaskColor` ([mask_brush](#mask_brushhpp)), `saveCurrentPreset` ([presets](#presetshpp)), `saveSidecar` ([saved_edits](#saved_editshpp)), `sectionLabel` ([panes/pane_helpers](#panespane_helpershpp)), `segmentObject` ([segmentation](#segmentationhpp)), `selectedIsBrush` ([masks](#maskshpp)), `selectedIsObject` ([virtual_copies](#virtual_copieshpp)), `selectedIsPolygon` ([masks](#maskshpp)), `selectedMask` ([masks](#maskshpp)), `selectedPart` ([masks](#maskshpp)), `selectedPartType` ([masks](#maskshpp)), `selectMask` ([masks](#maskshpp)), `selMask` ([masks](#maskshpp)), `selPart` ([masks](#maskshpp)), `setAngleDiscrete` ([rotate](#rotatehpp)), `setAngleFromSlider` ([rotate](#rotatehpp)), `setClipping` ([auto_tone_hist](#auto_tone_histhpp)), `setCornerMode` ([rotate](#rotatehpp)), `setCropAspect` ([crop](#crophpp)), `setCurveChannel` ([curve_editor](#curve_editorhpp)), `setDpr` ([canvas_view](#canvas_viewhpp)), `setLutAmount` ([lut_pane](#lut_panehpp)), `setMaskTintColor` ([masks](#maskshpp)), `setPartOp` ([mask_list](#mask_listhpp)), `setPhotoMeta` ([meta_commands](#meta_commandshpp)), `setSegInfo` ([segmentation](#segmentationhpp)), `setSelectedObjectMap` ([virtual_copies](#virtual_copieshpp)), `setStatus` ([editor_state](#editor_statehpp)), `setZoom` ([canvas_view](#canvas_viewhpp)), `shapeInView` ([masks](#maskshpp)), `sidebarButton` ([widgets](#widgetshpp)), `sidecarPath` ([saved_edits](#saved_editshpp)), `splitFromX` ([compare](#comparehpp)), `starsText` ([filmstrip](#filmstriphpp)), `startExportBatch` ([export](#exporthpp)), `stopExport` ([export](#exporthpp)), `submitGeometryJob` ([geometry_worker](#geometry_workerhpp)), `swallowPress` ([widgets](#widgetshpp)), `syncBusy` ([sync](#synchpp)), `syncMaskNameField` ([masks](#maskshpp)), `syncSettings` ([sync](#synchpp)), `syncSource` ([crop](#crophpp))  
**T** `tintFlag` ([masks](#maskshpp)), `titlebarIconBtn` ([widgets](#widgetshpp)), `toggleBefore` ([compare](#comparehpp)), `toggleChannelClipping` ([auto_tone_hist](#auto_tone_histhpp)), `toggleClipping` ([auto_tone_hist](#auto_tone_histhpp)), `toggleFilmstrip` ([filmstrip](#filmstriphpp)), `toggleSidebar` ([meta_commands](#meta_commandshpp)), `toggleSplit` ([compare](#comparehpp)), `toggleWbPicker` ([wb_picker](#wb_pickerhpp)), `typingInTextField` ([masks](#maskshpp))  
**U** `undo` ([rotate](#rotatehpp)), `updateAnglePreview` ([rotate](#rotatehpp)), `updateImageFromAdjustments` ([render_update](#render_updatehpp)), `updatePreviewFromAdjustments` ([render_update](#render_updatehpp))  
**V** `verifyAdjustmentSpecs` ([startup_checks](#startup_checkshpp)), `viewBarPress` ([canvas_view](#canvas_viewhpp)), `viewDragTo` ([canvas_view](#canvas_viewhpp)), `viewGeo` ([canvas_view](#canvas_viewhpp)), `viewLocal` ([canvas_view](#canvas_viewhpp)), `viewOrigin` ([canvas_view](#canvas_viewhpp)), `viewPanBegin` ([canvas_view](#canvas_viewhpp)), `viewThumb` ([canvas_view](#canvas_viewhpp))  
**W** `wbPickerActive` ([wb_picker](#wb_pickerhpp)), `wbPress` ([wb_picker](#wb_pickerhpp)), `wbSampleSpot` ([wb_picker](#wb_pickerhpp)), `winButton` ([widgets](#widgetshpp)), `writeTextAtomic` ([sync](#synchpp))  
**Z** `zoomAbout` ([canvas_view](#canvas_viewhpp))