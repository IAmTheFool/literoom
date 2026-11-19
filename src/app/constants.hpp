// app/constants.hpp: layout constants and the color palette
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Layout constants ----------------
constexpr float kTitlebarH = 36.0f;
constexpr float kStatusBarH = 26.0f;
constexpr float kActivityBarW = 48.0f;
constexpr float kDividerW = 4.0f;
constexpr float kMinSidebarW = 180.0f;
constexpr float kMaxSidebarW = 420.0f;

// Right-hand panel of the Masks pane (the selected mask's controls).
constexpr float kMinMaskPanelW = 300.0f;
constexpr float kMaxMaskPanelW = 480.0f;
constexpr float kMaskPanelPad = 12.0f; // panel padding, each side
constexpr float kMaskTrackW = 240.0f;  // slider track length in that panel
                                       // (kMinMaskPanelW - 2 * pad - scrollbar
                                       // must stay >= this)

// Zoom range/step. Zoom is measured in DEVICE pixels (see EditorState::zoomDev):
// 1.0 = one image pixel per physical screen pixel, on any display scale.
constexpr float kZoomMin = 0.1f;
constexpr float kZoomMax = 8.0f;
constexpr float kZoomStep = 1.25f;

// Crop tool. Sizes marked "screen px" are constant on screen regardless of
// zoom (the overlay divides by state.zoom when drawing/hit-testing).
constexpr float kCropMinPx = 16.0f;   // smallest selection edge, image px
constexpr float kCropHandlePx = 8.0f; // drawn handle size, screen px
constexpr float kCropHitPx = 10.0f;   // handle grab tolerance, screen px

// Color Mixer tab width (the engine constants live in color_mix.hpp).
constexpr float kMixTabW = 46.0f; // Hue/Sat/Lum tab width (3 + gaps = 150)

// Histogram canvas height (the counting constants live in histogram.hpp).
constexpr float kHistHeightPx = 80.0f;
// The same canvas is taller while it shows a waveform or a vectorscope (the
// vectorscope is a square, so it needs the room). See scopes.hpp.
constexpr float kScopeHeightPx = 120.0f;

// Clipping warnings: corner triangles on the histogram (the canvas overlay's
// constants live in histogram.hpp too).
constexpr float kHistTriPx = 9.0f;     // triangle leg length
constexpr float kHistTriHitPx = 18.0f; // click zone in each top corner

// Tone curve editor (canvas is square, kCurveSizePx on a side; the plot area
// is inset by kCurvePadPx so the end points aren't clipped).
constexpr float kCurveSizePx = 180.0f;
constexpr float kCurvePadPx = 8.0f;
constexpr float kCurveHitPx = 10.0f;  // point grab tolerance, screen px
constexpr float kCurvePointPx = 8.0f; // drawn point size, screen px
constexpr float kCurveTabW =
    42.0f; // channel tab width (4 tabs + gaps = canvas width)

// Document size before any image has been opened, so there's a visible
// page to scroll/zoom around from the start.
constexpr float kPlaceholderW = 800.0f;
constexpr float kPlaceholderH = 600.0f;

// Activity id of the History panel (see the activities list in main).
constexpr int kHistoryActivity = 9;
// Activity id of the Presets panel, and how many preset rows it can show (the
// view tree is built once, like the mask and history lists).
constexpr int kPresetsActivity = 10;
constexpr size_t kPresetRows = 24;
// Activity id of the Snapshots panel. It has kMaxSnapshots rows (sidecar.hpp);
// the view tree is built once, like the preset and history lists.
constexpr int kSnapshotsActivity = 11;

// Activity id of the LUTs panel. Like the preset list, its view tree is built
// once, with a row for every LUT the library can hold (kLutMaxFiles).
constexpr int kLutsActivity = 12;
constexpr size_t kLutRows = kLutMaxFiles;
constexpr float kLutRowH =
    kFilmThumbH + 12.0f; // a row with its preview of the picture
constexpr float kLutRowNamesH =
    28.0f; // a row with no picture open: just the name

// Masks, UI side: tint-bitmap sizes and overlay handle sizes. The engine
// constants live next to their algorithms: mask_geom.hpp (radii),
// mask_brush.hpp, mask_range.hpp, mask_object.hpp, seg_basic.hpp and mask.hpp
// (list limits).
constexpr int kCompTintEdge = 512;    // long side of a combined mask's tint
constexpr float kMaskHandlePx = 9.0f; // overlay handle size, screen px
constexpr float kMaskHitPx = 12.0f;   // overlay handle grab tolerance, px
constexpr int kBrushTintEdge = 512;   // long side of the brush tint bitmap
constexpr int kLumTintEdge = 512;     // long side of the on-canvas tint bitmap

// ---------------- Palette (dark, Lightroom-like) ----------------
// Base swatches.
constexpr Color kPanelBg{68, 68, 68};
constexpr Color kHeaderBg{40, 40, 40};
constexpr Color kTitle{176, 176, 176};
constexpr Color kLabel{160, 160, 160};
constexpr Color kValue{232, 232, 232};
constexpr Color kDivider{46, 46, 46};
constexpr Color kTrackFill{32, 32, 32};
constexpr Color kTrackEdge{104, 104, 104};
constexpr Color kThumbFill{196, 196, 196};
constexpr Color kThumbEdge{28, 28, 28};
constexpr Color kIconRing{205, 205, 205};
constexpr Color kIconActiveBg{92, 92, 92};
constexpr Color kTransparent{0, 0, 0, 0};

// Semantic names used throughout the UI, mapped onto the swatches above.
constexpr Color kColorTransparent = kTransparent;
constexpr Color kColorWhite{255, 255, 255, 255}; // real white (image, glyphs)

// Chrome backgrounds.
constexpr Color kColorRootBg = kHeaderBg;
constexpr Color kColorPanelBg = kPanelBg;         // sidebar
constexpr Color kColorTitlebarBg = kHeaderBg;     // titlebar
constexpr Color kColorActivityBarBg = kHeaderBg;  // activity bar, idle buttons
constexpr Color kColorMenuHover{62, 62, 62, 255}; // menu / win-btn hover
constexpr Color kColorActivityHover{84, 84, 84, 255}; // button / tab hover
constexpr Color kColorActivityActive = kIconActiveBg; // selected activity btn
constexpr Color kColorDivider = kDivider;             // divider bg
constexpr Color kColorTrack = kTrackFill;             // slider + scroll track
constexpr Color kColorScrollThumb = kTrackEdge;

// Text.
constexpr Color kColorTextDark = kValue;    // titles, menu labels
constexpr Color kColorTextMid = kTitle;     // info pane, win-btn glyphs
constexpr Color kColorTextIcon = kIconRing; // activity bar icons
constexpr Color kColorTextMuted = kLabel;   // section titles
constexpr Color kColorTextFaint{120, 120, 120, 255}; // placeholders

// Dropdown menu.
constexpr Color kColorDropdownBg = kHeaderBg;
constexpr Color kColorDropdownBorder = kThumbEdge;
constexpr Color kColorDropdownRowHover = kIconActiveBg; // also "selected"

// Window controls / accents.
constexpr Color kColorCloseHover{232, 17, 35, 255};
constexpr Color kColorDividerHover{100, 140, 220, 255}; // accent (drag, fills)
constexpr Color kColorAccent{0, 122, 204, 255};         // primary button
constexpr Color kColorAccentHover{30, 140, 220, 255};

// Canvas / viewport.
constexpr Color kColorCanvasPlaceholderBg{52, 52, 52, 255};
constexpr Color kColorViewportBg{30, 30, 30, 255};
constexpr Color kColorCanvasBorder = kTrackEdge;
constexpr Color kColorEmptyCorner{50, 50, 50, 255}; // behind transparent
                                                    // corners of a
                                                    // straightened image

// Status bar.
constexpr Color kColorStatusBarBg = kHeaderBg;
constexpr Color kColorStatusHover = kPanelBg;
constexpr Color kColorStatusError{255, 214, 102, 255};

// A sidebar tab in the activity bar.
struct Activity {
  int id;
  std::string title;
  std::string icon; // single glyph
};

// Display color of each Color Mixer band (UI only; the math is in
// color_mix.hpp).
static Color mixBandColor(size_t band) {
  static const Color colors[kMixBandCount] = {
      {230, 60, 60, 255},  {240, 140, 50, 255}, {225, 195, 40, 255},
      {70, 180, 80, 255},  {50, 185, 190, 255}, {60, 110, 230, 255},
      {140, 80, 210, 255}, {215, 70, 170, 255}};
  return colors[band];
}

