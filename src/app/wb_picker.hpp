// app/wb_picker.hpp: white-balance picker
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- White-balance picker ----------------
// Armed from the Adjust pane's button or the W key. Its math is
// solveWhiteBalance (color_pass.hpp); this is only the UI side: reading the
// spot under the click and reporting the result.

// Half-width of the square patch averaged around the click: 2 = 5 x 5 pixels,
// so one noisy pixel cannot decide the result.
constexpr int kWbPatchRadius = 2;

// True while the picker is armed and still makes sense. It only works in the
// Adjust pane (activity 1) on a shown picture, outside Before / After; once any
// of those stops being true it disarms itself, so it can never wake up later.
static bool wbPickerActive() {
  if (!state.wbPicking)
    return false;
  if (state.activeActivity != kAdjustActivity || !state.image ||
      state.compare != CompareMode::Off) {
    state.wbPicking = false;
    return false;
  }
  return true;
}

static void toggleWbPicker() {
  if (state.dragging)
    return;
  if (state.wbPicking) {
    state.wbPicking = false;
    clearStatus();
    state.markDirty();
    return;
  }
  if (!state.image)
    return;
  if (state.compare != CompareMode::Off) {
    setStatus("Leave Before / After to pick a white balance", true);
    return;
  }
  state.activeActivity = kAdjustActivity; // the picker lives in the Adjust pane
  state.wbPicking = true;
  setStatus("White balance: click a neutral gray in the picture (Esc cancels)",
            false);
  state.markDirty();
}

// The average color of the patch under canvas point (lx, ly), as the color pass
// will receive it: `source` (what the adjustments run on) through the current
// tone tables, before any white balance. False if the point is outside the
// picture or the patch has no visible pixel (straighten's empty corners).
static bool wbSampleSpot(float lx, float ly, float out[3]) {
  const float docW = state.docWidth(), docH = state.docHeight();
  const CanvasImage *src = state.source.get();
  if (!src || src->width <= 0 || src->height <= 0 || docW <= 0.0f ||
      docH <= 0.0f || state.zoom <= 0.0f)
    return false;
  const float dx = lx / state.zoom, dy = ly / state.zoom;
  if (dx < 0.0f || dy < 0.0f || dx >= docW || dy >= docH)
    return false;
  const int cx =
      std::clamp(static_cast<int>(dx / docW * src->width), 0, src->width - 1);
  const int cy =
      std::clamp(static_cast<int>(dy / docH * src->height), 0, src->height - 1);
  const auto luts = state.adjust.buildChannelLuts();
  double sum[3] = {0, 0, 0};
  int count = 0;
  for (int y = std::max(0, cy - kWbPatchRadius);
       y <= std::min(src->height - 1, cy + kWbPatchRadius); ++y)
    for (int x = std::max(0, cx - kWbPatchRadius);
         x <= std::min(src->width - 1, cx + kWbPatchRadius); ++x) {
      const uint8_t *p =
          src->pixels.data() + (static_cast<size_t>(y) * src->width + x) * 4;
      if (p[3] == 0)
        continue;
      sum[0] += luts[0][p[0]];
      sum[1] += luts[1][p[1]];
      sum[2] += luts[2][p[2]];
      ++count;
    }
  if (count == 0)
    return false;
  for (int c = 0; c < 3; ++c)
    out[c] = static_cast<float>(sum[c] / count);
  return true;
}

// A click while the picker is armed. Returns true if it consumed the press (so
// the crop and mask tools never see it). A click that cannot be used (outside
// the picture, too dark, clipped) keeps the picker armed so you can try again;
// a pick that sets the white balance disarms it.
static bool wbPress(float lx, float ly) {
  if (!wbPickerActive())
    return false;
  if (geometryBusy()) {
    setStatus("Wait for the changes to finish applying", true);
    return true;
  }
  float c[3];
  if (!wbSampleSpot(lx, ly, c))
    return true;
  const WbPick pick = solveWhiteBalance(c[0], c[1], c[2]);
  if (pick.status == WbPickStatus::Clipped) {
    setStatus(
        "That spot is clipped (pure white): pick a gray that is not blown out",
        true);
    return true;
  }
  if (!pick.usable()) {
    setStatus("That spot is too dark to read a color: pick a brighter gray",
              true);
    return true;
  }
  state.wbPicking = false;
  state.markDirty();
  if (!state.adjust.setWhiteBalance(pick.temperature, pick.tint)) {
    setStatus("White balance is already set for that spot", false);
    return true;
  }
  state.markAdjustDirty(); // re-render through the normal (drag-safe) path
  auto sgn = [](float v) {
    const int i = static_cast<int>(std::lround(v));
    return (i > 0 ? "+" : "") + std::to_string(i);
  };
  setStatus("White balance: Temperature " + sgn(pick.temperature) + ", Tint " +
                sgn(pick.tint) +
                (pick.status == WbPickStatus::Limited
                     ? " (that spot cannot be made fully gray)"
                     : ""),
            false);
  return true;
}

