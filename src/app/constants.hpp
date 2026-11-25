// app/constants.hpp: layout constants and the color palette
#pragma once

#include <cstddef>
#include <string>

#include "liteui.hpp"               // Color
#include "color/color_mix.hpp"      // kMixBandCount
#include "io/lut_library.hpp"       // kLutMaxFiles
#include "workers/thumb_worker.hpp" // kFilmThumbW, kFilmThumbH

// ---------------- Layout constants ----------------
inline constexpr float kTitlebarH = 36.0f;
inline constexpr float kStatusBarH = 26.0f;
inline constexpr float kActivityBarW = 48.0f;
inline constexpr float kDividerW = 4.0f;
inline constexpr float kMinSidebarW = 180.0f;
inline constexpr float kMaxSidebarW = 420.0f;

// Right-hand panel of the Masks pane (the selected mask's controls).
inline constexpr float kMinMaskPanelW = 300.0f;
inline constexpr float kMaxMaskPanelW = 480.0f;
inline constexpr float kMaskPanelPad = 12.0f; // panel padding, each side
inline constexpr float kMaskTrackW = 240.0f;  // slider track length in that panel
                                       // (kMinMaskPanelW - 2 * pad - scrollbar
                                       // must stay >= this)

// Zoom range/step. Zoom is measured in DEVICE pixels (see
// EditorState::zoomDev): 1.0 = one image pixel per physical screen pixel, on
// any display scale.
inline constexpr float kZoomMin = 0.1f;
inline constexpr float kZoomMax = 8.0f;
inline constexpr float kZoomStep = 1.25f;

// Crop tool. Sizes marked "screen px" are constant on screen regardless of
// zoom (the overlay divides by state.zoom when drawing/hit-testing).
inline constexpr float kCropMinPx = 16.0f;   // smallest selection edge, image px
inline constexpr float kCropHandlePx = 8.0f; // drawn handle size, screen px
inline constexpr float kCropHitPx = 10.0f;   // handle grab tolerance, screen px

// Color Mixer tab width (the engine constants live in color_mix.hpp).
inline constexpr float kMixTabW = 46.0f; // Hue/Sat/Lum tab width (3 + gaps = 150)

// Histogram canvas height (the counting constants live in histogram.hpp).
inline constexpr float kHistHeightPx = 80.0f;
// The same canvas is taller while it shows a waveform or a vectorscope (the
// vectorscope is a square, so it needs the room). See scopes.hpp.
inline constexpr float kScopeHeightPx = 120.0f;

// Clipping warnings: corner triangles on the histogram (the canvas overlay's
// constants live in histogram.hpp too).
inline constexpr float kHistTriPx = 9.0f;     // triangle leg length
inline constexpr float kHistTriHitPx = 18.0f; // click zone in each top corner

// Tone curve editor (canvas is square, kCurveSizePx on a side; the plot area
// is inset by kCurvePadPx so the end points aren't clipped).
inline constexpr float kCurveSizePx = 180.0f;
inline constexpr float kCurvePadPx = 8.0f;
inline constexpr float kCurveHitPx = 10.0f;  // point grab tolerance, screen px
inline constexpr float kCurvePointPx = 8.0f; // drawn point size, screen px
inline constexpr float kCurveTabW =
    42.0f; // channel tab width (4 tabs + gaps = canvas width)

// Document size before any image has been opened, so there's a visible
// page to scroll/zoom around from the start.
inline constexpr float kPlaceholderW = 800.0f;
inline constexpr float kPlaceholderH = 600.0f;

inline constexpr size_t kPresetRows = 24;

inline constexpr size_t kLutRows = kLutMaxFiles; 
inline constexpr float kLutRowH =
    kFilmThumbH + 12.0f; // a row with its preview of the picture
inline constexpr float kLutRowNamesH =
    28.0f; // a row with no picture open: just the name

// Masks, UI side: tint-bitmap sizes and overlay handle sizes. The engine
// constants live next to their algorithms: mask_geom.hpp (radii),
// mask_brush.hpp, mask_range.hpp, mask_object.hpp, seg_basic.hpp and mask.hpp
// (list limits).
inline constexpr int kCompTintEdge = 512;    // long side of a combined mask's tint
inline constexpr float kMaskHandlePx = 9.0f; // overlay handle size, screen px
inline constexpr float kMaskHitPx = 12.0f;   // overlay handle grab tolerance, px
inline constexpr int kBrushTintEdge = 512;   // long side of the brush tint bitmap
inline constexpr int kLumTintEdge = 512;     // long side of the on-canvas tint bitmap

// ---------------- Palette (dark, Lightroom-like) ----------------
// Base swatches.
inline constexpr Color kPanelBg{68, 68, 68};
inline constexpr Color kHeaderBg{40, 40, 40};
inline constexpr Color kTitle{176, 176, 176};
inline constexpr Color kLabel{160, 160, 160};
inline constexpr Color kValue{232, 232, 232};
inline constexpr Color kDivider{46, 46, 46};
inline constexpr Color kTrackFill{32, 32, 32};
inline constexpr Color kTrackEdge{104, 104, 104};
inline constexpr Color kThumbFill{196, 196, 196};
inline constexpr Color kThumbEdge{28, 28, 28};
inline constexpr Color kIconRing{205, 205, 205};
inline constexpr Color kIconActiveBg{92, 92, 92};
inline constexpr Color kTransparent{0, 0, 0, 0};

// Semantic names used throughout the UI, mapped onto the swatches above.
inline constexpr Color kColorTransparent = kTransparent;
inline constexpr Color kColorWhite{255, 255, 255, 255}; // real white (image, glyphs)

// Chrome backgrounds.
inline constexpr Color kColorRootBg = kHeaderBg;
inline constexpr Color kColorPanelBg = kPanelBg;         // sidebar
inline constexpr Color kColorTitlebarBg = kHeaderBg;     // titlebar
inline constexpr Color kColorActivityBarBg = kHeaderBg;  // activity bar, idle buttons
inline constexpr Color kColorMenuHover{62, 62, 62, 255}; // menu / win-btn hover
inline constexpr Color kColorActivityHover{84, 84, 84, 255}; // button / tab hover
inline constexpr Color kColorActivityActive = kIconActiveBg; // selected activity btn
inline constexpr Color kColorDivider = kDivider;             // divider bg
inline constexpr Color kColorTrack = kTrackFill;             // slider + scroll track
inline constexpr Color kColorScrollThumb = kTrackEdge;

// Text.
inline constexpr Color kColorTextDark = kValue;    // titles, menu labels
inline constexpr Color kColorTextMid = kTitle;     // info pane, win-btn glyphs
inline constexpr Color kColorTextIcon = kIconRing; // activity bar icons
inline constexpr Color kColorTextMuted = kLabel;   // section titles
inline constexpr Color kColorTextFaint{120, 120, 120, 255}; // placeholders

// Dropdown menu.
inline constexpr Color kColorDropdownBg = kHeaderBg;
inline constexpr Color kColorDropdownBorder = kThumbEdge;
inline constexpr Color kColorDropdownRowHover = kIconActiveBg; // also "selected"

// Window controls / accents.
inline constexpr Color kColorCloseHover{232, 17, 35, 255};
inline constexpr Color kColorDividerHover{100, 140, 220, 255}; // accent (drag, fills)
inline constexpr Color kColorAccent{0, 122, 204, 255};         // primary button
inline constexpr Color kColorAccentHover{30, 140, 220, 255};

// Canvas / viewport.
inline constexpr Color kColorCanvasPlaceholderBg{52, 52, 52, 255};
inline constexpr Color kColorViewportBg{30, 30, 30, 255};
inline constexpr Color kColorCanvasBorder = kTrackEdge;
inline constexpr Color kColorEmptyCorner{50, 50, 50, 255}; // behind transparent
                                                    // corners of a
                                                    // straightened image

// Status bar.
inline constexpr Color kColorStatusBarBg = kHeaderBg;
inline constexpr Color kColorStatusHover = kPanelBg;
inline constexpr Color kColorStatusError{255, 214, 102, 255};

// A sidebar tab in the activity bar.
struct Activity {
  int id;
  std::string title;
  std::string icon; // single glyph
};

// Display color of each Color Mixer band (UI only; the math is in
// color_mix.hpp).
inline Color mixBandColor(size_t band) {
  static const Color colors[kMixBandCount] = {
      {230, 60, 60, 255},  {240, 140, 50, 255}, {225, 195, 40, 255},
      {70, 180, 80, 255},  {50, 185, 190, 255}, {60, 110, 230, 255},
      {140, 80, 210, 255}, {215, 70, 170, 255}};
  return colors[band];
}

enum ActivityId : int {
  kNeverSeenActivity = -2, // sentinel for "no tab observed yet"; never a real state
  kNoActivity = -1,
  kInfoActivity = 0,
  kAdjustActivity = 1,
  kCropActivity = 2,
  kRotateActivity = 3,
  kCurveActivity = 4,
  kColorMixActivity = 5,
  kColorGradingActivity = 6,
  kMasksActivity = 7,
  kExportActivity = 8,
  kHistoryActivity = 9,
  kPresetsActivity = 10,
  kSnapshotsActivity = 11,
  kLutsActivity = 12,
};