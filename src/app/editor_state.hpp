// app/editor_state.hpp: crop / filmstrip / compare state and EditorState (the
// global `state`)

#pragma once

#include <algorithm>   // std::clamp
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "liteui.hpp"               // CanvasImage, CanvasTexture

#include "core/geometry.hpp"        // Orient, CropRect
#include "core/image16.hpp"         // Image16
#include "edit/adjustment_set.hpp"  // AdjustmentSet
#include "io/color_tag.hpp"         // color_tag::ColorTag
#include "io/exif.hpp"              // exif::PhotoInfo
#include "io/photo_meta.hpp"        // PhotoMeta, MetaFilter
#include "io/sidecar.hpp"           // SnapshotList
#include "mask/mask.hpp"            // MaskPart
#include "mask/mask_brush.hpp"      // BrushShape, kBrushDefaultSize
#include "mask/mask_object.hpp"     // ObjectPrompt
#include "mask/mask_polygon.hpp"    // PolyPt
#include "mask/mask_range.hpp"      // LuminosityShape
#include "render/scopes.hpp"        // scopes::Kind

#include "app/constants.hpp"        // kInfoActivity, kPlaceholderW/H, kZoomMin/Max
#include "app/workers.hpp"          // workers(), see below

// ---------------- Crop tool state ----------------

// Selection-drag "handle" bitmask: which edges of the selection a drag is
// moving. Two bits (e.g. L|T) = a corner; one bit = an edge; kEdgeMove =
// the whole rectangle.
inline constexpr int kEdgeL = 1, kEdgeR = 2, kEdgeT = 4, kEdgeB = 8;
inline constexpr int kEdgeMove = 16;

// Aspect-ratio presets in the Crop pane. ratio (width / height): 0 = free,
// negative = "same as the image being cropped".
struct CropAspect {
  const char *label;
  float ratio;
};
inline const std::array<CropAspect, 6> kCropAspects = {{
    {"Free", 0.0f},
    {"Original", -1.0f},
    {"1:1", 1.0f},
    {"4:3", 4.0f / 3.0f},
    {"3:2", 3.0f / 2.0f},
    {"16:9", 16.0f / 9.0f},
}};

// The live (not yet applied) selection rectangle and the drag currently
// editing it. Coordinates are in pixels of the image being shown
// (EditorState::image), not the original.
struct CropUi {
  float l = 0, t = 0, r = 0, b = 0;
  size_t aspectIdx = 0; // index into kCropAspects

  enum class Drag { None, New, Move, Resize } drag = Drag::None;
  int mask = 0;               // Resize: which edges (kEdge* bits)
  float ax = 0, ay = 0;       // New: the fixed anchor corner
  float grabX = 0, grabY = 0; // Move: pointer offset from selection's l/t
};

// Mask-overlay drag in progress (see maskPress / maskDrag).
struct MaskUi {
  enum class Drag {
    None,
    Move,
    ResizeX,
    ResizeY,
    LinEnd,
    LinStart,
    Paint,      // a brush stroke is being painted
    PolyVertex, // a polygon vertex is being dragged (see polyIdx)
    PolyMove,   // the whole polygon is being dragged
    Lasso       // a freehand lasso is being drawn
  } drag = Drag::None;
  float grabX = 0, grabY = 0; // Move: pointer offset from the mask's center

  // Brush tool. These settings apply to the NEXT stroke; every stroke keeps
  // the values it was painted with. Size is a radius, as a fraction of the
  // SHORTER side of the picture on screen.
  float brushSize = kBrushDefaultSize;
  float brushFeather = 0.5f;
  float brushDensity = 1.0f;
  float brushFlow = 1.0f;
  bool brushErase = false;
  bool brushTint =
      true; // paint the selected brush's coverage in the overlay color
  float lastX = 0, lastY = 0; // Paint: last recorded point, document px
  float curX = 0, curY = 0;   // Paint: pointer, document px (cursor ring)

  // Object tool.
  bool objBg = false; // plain clicks add BACKGROUND points (touchpad-friendly
                      // alternative to right-click)
  struct ObjPending {
    int mask = -1, part = -1;
    std::vector<ObjectPrompt> prompts; // original-picture fractions
  } objPending;

  // Polygon tool. Clicks place / drag / insert vertices; with polyLasso on, a
  // drag draws a freehand outline instead (simplified live while it is drawn).
  bool polyLasso = false;
  int polyIdx = -1;             // PolyVertex: the vertex being dragged
  std::vector<PolyPt> lassoRaw; // Lasso: the pointer's path, document px

  // The red coverage bitmap of the selected brush mask, rebuilt only when the
  // strokes (or the picture's size) change.
  std::shared_ptr<CanvasImage> tint;
  BrushShape tintShape;
  bool tintInvert = false;

  // The red selection bitmap of the selected luminosity mask. It is built
  // from the picture as it stands just BEFORE that mask (see
  // luminosityTintImage), so it is rebuilt when the shape, the mask's place in
  // the list, or the pixels change. The pixels are keyed by the identity of
  // the images on screen; weak pointers, so no picture is kept alive and no
  // address can be mistaken for a newer image that reuses it.
  std::shared_ptr<CanvasImage> lumTint;
  LuminosityShape lumShape;
  bool lumInvert = false;
  int lumSel = -1;
  std::weak_ptr<CanvasImage> lumKeyImage, lumKeyPreview;
  // Small stand-in for state.source the tint is computed on.
  std::shared_ptr<CanvasImage> lumBase;
  std::weak_ptr<CanvasImage> lumBaseSrc;

  // Which part of the selected mask the canvas and the shape sliders edit.
  // Read it through selPart(), which keeps it in range.
  int part = 0;

  // The red bitmap of a selected mask with SEVERAL parts: the whole
  // combination, evaluated like the renderer does (see compositeTintImage).
  std::shared_ptr<CanvasImage> compTint;
  std::vector<MaskPart> compParts; // in-view form, the cache key
  bool compInvert = false;
  bool compUsesPix = false;
  int compSel = -1, compW = 0, compH = 0;
  std::weak_ptr<CanvasImage> compKeyImage, compKeyPreview;
  uint64_t encodedKey = 0; // imageSerial the segmenter has already analyzed
};

// ---------------- Filmstrip and Before / After state ----------------
// One picture in the open folder, as the filmstrip shows it.
struct FilmEntry {
  std::string path;                         // full path (UTF-8)
  std::string name;                         // file name only
  std::shared_ptr<const CanvasImage> thumb; // null until made
  // The GPU-side copy of `thumb` that paintFilmstrip draws (see
  // filmThumbTexture). Made lazily while painting, hence mutable.
  mutable std::shared_ptr<CanvasTexture> thumbTex;
  mutable std::shared_ptr<const CanvasImage> thumbTexOf; // what thumbTex shows
  bool failed = false;   // couldn't be decoded: plain box
  bool edited = false;   // has saved edits (a sidecar file)
  bool selected = false; // in the multi-selection (Sync Settings)
  PhotoMeta meta;        // rating / flag / color label
  bool scanned = false;  // `meta` has been read from the sidecar
  int copy = 0;          // 0: the picture's own edits; n: virtual
                         // copy n (its `path` is "<file>|vc<n>",
                         // see virtual_copy.hpp; `name` is the file's)
};

// The pictures of the folder the open picture lives in (JPEG / PNG, natural
// name order), plus the strip's scroll position. UI thread only.
struct FilmStrip {
  std::vector<FilmEntry> files;
  std::unordered_map<std::string, size_t> byName; // lower-case name -> index
  std::string folder;
  int current = -1;          // index of the picture that is open
  int target = -1;           // the one last asked for (== current when idle)
  int anchor = -1;           // where a Shift-click range starts
  float scrollX = 0.0f;      // pixels scrolled
  float viewW = 0.0f;        // strip width at the last paint
  bool visible = true;       // View > Toggle Filmstrip
  bool dirty = true;         // repaint wanted
  uint64_t thumbVersion = 0; // adjust.version() when the open picture's
                             // thumbnail was last (re)made
  MetaFilter filter;         // the filter bar's state (rating / flag / label)
  std::vector<int> view;     // indices into `files` the strip shows, ascending
                             // (see filmRebuildView)
};

// What the canvas shows: the edited picture, the same picture with every
// edit switched off (crop and rotation stay), or both side by side.
enum class CompareMode { Off, Before, Split };

struct EditorState {
  std::shared_ptr<CanvasImage> original; // untouched decoded pixels; null
                                         // until something is opened
  std::shared_ptr<CanvasImage> oriented; // original rotated/flipped per
                                         // adjust.orient() (== original if
                                         // none); crops are measured on this
  Orient orientedFor;                    // the orientation `oriented` has
  std::shared_ptr<CanvasImage>
      orientedPreview;                  // `oriented` downsampled for
                                        // live angle drags (null if small)
  std::shared_ptr<CanvasImage> rotated; // oriented, straightened by the
                                        // angle (== oriented at 0 degrees);
                                        // crops are measured on this
  float rotatedAngle = 0.0f;            // the angle `rotated` was built with
  bool rotatedExpand = false;           // ...and its corner mode
  std::shared_ptr<CanvasImage> source;  // rotated with the committed crop
                                        // applied (== rotated if none) —
                                        // what adjustments are run on
  CropRect sourceCrop;                  // the crop `source` was built with
  // The 16-bit twin of original / oriented / rotated / source, for a picture
  // that really has more than 8 bits (a 16-bit PNG); all null for any other
  // file. Built from `original16` by the same operations, so
  // each stage has the size of its 8-bit namesake (core/wide_chain.hpp). The
  // 8-bit chain above is unchanged and is still what every tool measures; the
  // full-resolution render (and 16-bit export) starts from the twin, and the
  // neighbor effects read it directly. The drag preview starts from
  // previewBase16 when there is one.
  std::shared_ptr<Image16> original16, oriented16, rotated16, source16;
  std::shared_ptr<CanvasImage> image; // source + adjustments applied —
                                      // this is what's drawn and exported
  std::string imagePath;              // full path of the opened file
  exif::PhotoInfo
      photoInfo; // its camera data (Info pane); see currentPhotoInfo()
  uint64_t photoInfoSerial =
      ~uint64_t{0};             // the imageSerial photoInfo was read for
  color_tag::ColorTag colorTag; // what the open file says its color space is
                                // (Info pane); see currentColorTag()
  uint64_t colorTagSerial =
      ~uint64_t{0}; // the imageSerial colorTag was read for
  bool colorNoticePending =
      false; // the open's color notice may still be wiped by a geometry job;
             // show it again when that lands
  // Zoom. Everything on screen (the canvas size, overlays, hit-testing) is in
  // LOGICAL units, so `zoom` is logical units per image pixel, and every use of
  // it (lx / state.zoom, ctx.scale(state.zoom), ...) stays correct on a HiDPI
  // display. What the user sees ("100%") is device-based: zoomDev is device
  // pixels per image pixel, so 100% really is one image pixel per physical
  // pixel (pixel-exact), and zoom = zoomDev / dpr. Change them only through
  // applyZoomDev() / setZoom() / setDpr().
  float zoom = 1.0f;
  float zoomDev = 1.0f;
  float dpr = 1.0f; // device pixels per logical unit (LiteUI::contentScale())
  // The canvas is as big as the area it fills (not as big as the zoomed
  // picture), so its texture stays window-sized at any zoom. The picture is
  // drawn into it at an offset that we compute: centered when it fits, scrolled
  // by scrollX/scrollY (logical units, in the padded picture) when it doesn't.
  struct View2D {
    float w = 0, h = 0; // canvas size, from onLayout
    float scrollX = 0, scrollY = 0;
    enum class Drag { None, Pan, VThumb, HThumb } drag = Drag::None;
    float dragX = 0, dragY = 0;             // pointer at press
    float dragScrollX = 0, dragScrollY = 0; // scroll at press
  } view;
  AdjustmentSet adjust;      // current slider values + their undo/redo history
  uint64_t sidecarSaved = 0; // adjust.version() at the last sidecar save / load
  PhotoMeta meta;            // the open picture's rating / flag / color label
                             // (not an edit: no undo step, see ratePhoto)
  bool metaDirty = false;    // meta changed since the last sidecar save / load
  SnapshotList snapshots;    // the open picture's named snapshots: saved in its
                             // sidecar, not edits (see addSnapshot)
  bool snapshotsDirty =
      false; // snapshots changed since the last sidecar save / load
  bool sidecarBlocked =
      false; // the open picture's saved-edits file exists but could not be
             // read (see loadSidecar): saveSidecar must not replace it

  // Export pane settings.
  bool exportJpeg = true;   // the format the Export dialog offers first
  bool exportPng16 = false; // PNG export writes 16 bits per channel (the pane's
                            // "PNG  16-bit"); never set with exportJpeg
  int jpegQuality = 92;     // 1..100
  bool keepMetadata = true; // carry the photo's EXIF + ICC profile into a JPEG
  bool removeGps =
      false; // ...but without the GPS location (JPEG only; needs keepMetadata)
  bool exportResize =
      false; // shrink the export so its long side is exportLongEdge
  int exportLongEdge =
      2048; // px, kExportLongEdgeMin..kExportLongEdgeMax; never enlarges
  bool exportOverwrite =
      false; // batch export: replace files that exist (else "-2", "-3"...)

  FilmStrip film; // the open folder's pictures (bottom strip)
  CompareMode compare = CompareMode::Off; // Before / After view
  float splitPos = 0.5f; // Split view: line position, 0..1 of width
  bool dirty = true;

  // Set by a slider drag (many events per second while held). The actual
  // pixel recompute is deferred until canvasDirtySource is polled — which
  // happens at most once per rendered frame — instead of running once per
  // drag event, so a flurry of mouse-move events between two frames only
  // costs one recompute, not one per event.
  bool adjustDirty = false;

  // Interactive-preview path for large images. While a slider is being
  // dragged, adjustments are applied to `previewBase` (`source`
  // box-downsampled to roughly kPreviewMaxEdge on its long side) instead
  // of the full-resolution original, and the result (`previewImage`) is
  // drawn stretched to the document's full size. One full-resolution
  // recompute runs when the drag ends (pointer up / cancel). `previewBase` is
  // null for images already small enough that a preview wouldn't be
  // cheaper, in which case dragging just uses the full-resolution path.
  std::shared_ptr<CanvasImage> previewBase;
  // The 16-bit twin of `previewBase` (box-downsampled from `source16` by the
  // same factor, so the same size), for a picture that has a 16-bit twin; null
  // otherwise. A drag renders it, so the preview
  // starts from the same precision as the full-resolution render that follows
  // it. `previewBase` is then only its 8-bit view.
  std::shared_ptr<Image16> previewBase16;
  std::shared_ptr<CanvasImage> previewImage;
  bool dragging = false;

  CropUi crop; // live crop selection; only shown/editable while the Crop pane
               // (activity 2) is open

  // Masks pane (activity 7). The masks themselves live in `adjust` (so they
  // are undoable); this is only which one is selected and how it is shown.
  int maskSel = -1; // index into adjust.masks(); read it through selMask()
  bool maskShowTint =
      false; // paint the selected mask's weight in the overlay color
  int maskTintColor =
      0; // index into kMaskTintPalette (session only, not saved)
  MaskUi mask;

  int activeActivity = kInfoActivity; // kNoActivity = sidebar closed
  float sidebarWidth = 220.0f;
  float maskPanelW = 300.0f; // right-hand mask panel (Masks pane only)
  int maskTab = 0;           // its tab: 0 Effect, 1 Shape, 2 Parts
  int maskFx = 0;            // Effect tab: 0 Basic, 1 Mix, 2 Grade, 3 Curve
  int openMenu = -1;         // -1 = no dropdown open, else its menu id
  bool maximized = false;    // mirrors our own toggle (no getter in LiteUI)

  // One-line message shown in the status bar after the image info: open and
  // export failures, and export success. Cleared by the next open, export or
  // edit (see clearStatus's callers).
  std::string statusMsg;
  bool statusIsError = false;

  // While an angle drag shows its low-res preview, the picture's size is
  // changing every frame but `image` (full-res) isn't rebuilt until the drag
  // ends — so the document size comes from these overrides instead.
  float previewDocW = 0.0f, previewDocH = 0.0f;
  bool dragIsAngle = false; // the drag in progress is the angle slider

  uint64_t imageSerial =
      0; // bumps on every opened file (keys segmenter caches)

  // True while `previewImage` stands in for `image`: during a drag, and after
  // it until the background full-resolution render lands (workers().render).
  bool showPreview() const {
    return previewImage && (dragging || workers().render.outstanding());
  }

  bool previewDocActive() const {
    return dragging && previewImage && previewDocW > 0.0f;
  }
  float docWidth() const {
    if (previewDocActive())
      return previewDocW;
    return image ? static_cast<float>(image->width) : kPlaceholderW;
  }
  float docHeight() const {
    if (previewDocActive())
      return previewDocH;
    return image ? static_cast<float>(image->height) : kPlaceholderH;
  }
  void markDirty() { dirty = true; }
  void markAdjustDirty() { adjustDirty = true; }

  // Histogram of what is currently shown (the mid-drag preview while a
  // slider is held, else the full-resolution result). Recomputed lazily by
  // the histogram canvas's dirty poll when `histStale` is set; `histVersion`
  // bumps on every recompute so that canvas knows to repaint.
  std::array<uint32_t, 256> histR{}, histG{}, histB{}, histL{};
  bool histStale = true;
  uint32_t histVersion = 0;

  // What the pinned canvas above the panes shows; a click on it cycles
  // Histogram -> Waveform -> Vectorscope (cycleScope). scopeImage is the
  // waveform or vectorscope picture, rebuilt by computeHistogram() together
  // with the histogram; it is null while the histogram itself is showing (and
  // for an empty picture), so the histogram mode costs nothing extra.
  scopes::Kind scopeKind = scopes::Kind::Histogram;
  std::shared_ptr<CanvasImage> scopeImage;

  // Clipping warnings. The flags say which overlays are on; histClipLo/Hi
  // are the channels (bit 0/1/2 = R/G/B) that have any clipped pixel in the
  // shown picture (drives the triangle colors). clipPerChannel colors the
  // overlay by clipped channel (Shift+J) instead of classic red/blue.
  // clipOverlay is rebuilt by computeHistogram and is null while both
  // overlays are off.
  bool clipShadowsOn = false, clipHighlightsOn = false;
  uint8_t histClipLo = 0, histClipHi = 0;
  bool clipPerChannel = false;
  std::shared_ptr<CanvasImage> clipOverlay;

  // RGB readout under the cursor (status bar). hoverValid is false while
  // the pointer isn't over a picture pixel.
  bool hoverValid = false;
  uint8_t hoverR = 0, hoverG = 0, hoverB = 0;

  // White-balance picker (a button in the Adjust pane, key W). While armed, one
  // click on the picture sets Temperature and Tint so that spot turns gray.
  // Read it through wbPickerActive(), which also drops it when it no longer
  // applies.
  bool wbPicking = false;
};

inline EditorState state;

// Shows `msg` in the status bar. Kept to one short line: newlines (library
// error strings sometimes have them) become spaces, and long text is cut on
// a UTF-8 boundary with an ellipsis.
inline void setStatus(std::string msg, bool isError) {
  constexpr size_t kMaxLen = 110;
  for (char &c : msg)
    if (c == '\n' || c == '\r')
      c = ' ';
  if (msg.size() > kMaxLen) {
    size_t cut = kMaxLen - 3;
    while (cut > 0 && (static_cast<unsigned char>(msg[cut]) & 0xC0) == 0x80)
      --cut; // don't split a multi-byte character
    msg.resize(cut);
    msg += "...";
  }
  state.statusMsg = std::move(msg);
  state.statusIsError = isError;
}
inline void clearStatus() {
  state.statusMsg.clear();
  state.statusIsError = false;
}

// Sets the zoom from a device-pixel zoom (1.0 = 100%, pixel-exact) and derives
// the logical scale everything else uses. Does not mark dirty.
inline void applyZoomDev(float zoomDev) {
  state.zoomDev = std::clamp(zoomDev, kZoomMin, kZoomMax);
  state.zoom = state.zoomDev / state.dpr;
}