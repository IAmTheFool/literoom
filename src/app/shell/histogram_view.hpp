// app/shell/histogram_view.hpp: the pinned histogram / scopes view
#pragma once

#include <cstdint>    // uint32_t
#include <functional> // std::function

#include "liteui.hpp" // View, Size, EdgeInsets, Cursor, CanvasContext,
                      // PointerEvent, MouseButton

#include "app/auto_tone_hist.hpp" // paintHistogram, cycleScope, setClipping,
                                  // refreshHistogramIfStale
#include "app/constants.hpp"    // kHistHeightPx, kScopeHeightPx, kHistTriHitPx,
                                // kNeverSeenActivity
#include "app/editor_state.hpp" // inline EditorState state, scopes::Kind (?)
#include "app/ui_state.hpp"     // uiState
#include "app/widgets.hpp"      // closeMenus
// Histogram of the picture as currently shown (see computeHistogram).
// Pinned above every pane, like Lightroom. A click cycles it through the
// histogram, a waveform and a vectorscope (cycleScope).
inline View buildHistogramView() {
  View histogram;
  histogram.isCanvas = true;
  histogram.style.width = Size::full();
  histogram.style.height = std::function<Size()>([] {
    return Size::pixel(state.scopeKind == scopes::Kind::Histogram
                           ? kHistHeightPx
                           : kScopeHeightPx);
  });
  histogram.style.flexShrink = 0;
  histogram.style.margin = EdgeInsets{0, 0, 12, 0};
  histogram.onPaint = [](CanvasContext &ctx) { paintHistogram(ctx); };
  histogram.style.cursor = Cursor::Pointer;
  histogram.onPointerDown = [](PointerEvent &e) {
    if (e.button != MouseButton::Left)
      return;
    e.stopPropagation();
    closeMenus();
    const float lx = e.x, ly = e.y;
    if (!state.image)
      return;
    // The top corners toggle the clipping overlays; anywhere else cycles the
    // view (histogram / waveform / vectorscope).
    if (ly <= kHistTriHitPx && lx <= kHistTriHitPx)
      setClipping(!state.clipShadowsOn, state.clipHighlightsOn);
    else if (ly <= kHistTriHitPx && uiState.hist.canvasW > 0.0f && // painted
             lx >= uiState.hist.canvasW - kHistTriHitPx)
      setClipping(state.clipShadowsOn, !state.clipHighlightsOn);
    else
      cycleScope();
  };
  histogram.canvasDirtySource = [] {
    static uint32_t lastPainted = 0;
    static int lastActivity = kNeverSeenActivity;
    // Sidebar is polled before the canvas: do the pending recompute now.
    refreshHistogramIfStale();
    // Also repaint when the sidebar is switched or reopened, in case it
    // was hidden while the picture changed.
    const bool tabChanged = state.activeActivity != lastActivity;
    lastActivity = state.activeActivity;
    if (state.histVersion == lastPainted && !tabChanged)
      return false;
    lastPainted = state.histVersion;
    return true;
  };

  return histogram;
}
