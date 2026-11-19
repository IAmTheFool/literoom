// app/auto_tone_hist.hpp: Auto Tone, flushPendingAdjust, histogram and scopes
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Auto Tone ----------------
// The Adjust pane's Auto Tone button, the A key and Image > Auto Tone. The math
// is solveAutoTone (core/auto_tone.hpp); the edit is AdjustmentSet::setAutoTone
// (Exposure, Contrast, Whites and Blacks in one undo step). This is only the UI
// side: measuring the picture and reporting the result.
//
// Measured on `source`, the picture the adjustments run on, NOT on the shown
// picture: the shown one already has the tone sliders applied, so its histogram
// would change every time Auto Tone ran. Reading `source` makes the answer the
// same whatever the sliders say, so pressing it twice is a no-op.
static void autoTone() {
  if (state.dragging || !state.source)
    return;
  if (geometryBusy()) {
    setStatus("Wait for the changes to finish applying", true);
    return;
  }
  const histogram::Counts c = histogram::compute(state.source.get());
  const AutoTone at = solveAutoTone(c.l);
  if (at.status == AutoToneStatus::NoPixels) {
    setStatus("Auto Tone: nothing to measure in this picture", true);
    return;
  }
  if (at.status == AutoToneStatus::Flat) {
    setStatus("Auto Tone: this picture has almost no tonal range to adjust",
              true);
    return;
  }
  if (!state.adjust.setAutoTone(at.exposure, at.contrast, at.whites,
                                at.blacks)) {
    setStatus("Auto Tone: the tone is already set for this picture", false);
    return;
  }
  state.markAdjustDirty(); // re-render through the normal (drag-safe) path
  state.markDirty();
  auto sgn = [](float v) {
    const int i = static_cast<int>(std::lround(v));
    return (i > 0 ? "+" : "") + std::to_string(i);
  };
  setStatus("Auto Tone: Exposure " + sgn(at.exposure) + ", Contrast " +
                sgn(at.contrast) + ", Whites " + sgn(at.whites) + ", Blacks " +
                sgn(at.blacks) +
                (at.status == AutoToneStatus::Limited
                     ? " (the slider ranges are not enough to go further)"
                     : ""),
            false);
}

// Runs the deferred, expensive part of a slider drag (rebuilding the shown
// image) exactly once per pending change. Idempotent: the main canvas and the
// histogram canvas both call it from their dirty polls, and whichever is
// polled first (the sidebar, so usually the histogram) does the work — so the
// histogram never lags a frame behind the picture.
static void flushPendingAdjust() {
  collectFullRender();  // a finished background render, if any
  collectGeometryJob(); // a finished background geometry rebuild, if any
  collectDecodeJob();   // a finished background file decode, if any
  collectSegmentJob();
  collectSyncJob();   // a finished Sync Settings batch, if any
  collectExportJob(); // progress / the end of a batch export, if any
  pollLutPreviews();  // LUT previews: results in, a new batch when the picture
                      // or its look changed
  autosaveSidecar();  // a finished change, once the mouse is released
  if (!state.adjustDirty)
    return;
  state.adjustDirty = false;
  LITEUI_PROF_START(profRecompute);
  if (state.dragging && state.dragIsAngle && state.orientedPreview)
    updateAnglePreview(); // cheap: turns the small preview, mid-drag
  else if (state.dragging && !state.dragIsAngle && state.previewBase)
    updatePreviewFromAdjustments(); // cheap: small image, mid-drag
  else
    updateImageFromAdjustments(); // full-res: rebuilds state.image
  LITEUI_PROF_ADD(recompute, profRecompute);
}

// Brings the shown image (and, if stale, the histogram) up to date. Several
// canvases poll this; only the first poll of a frame does any work.
static void refreshHistogramIfStale();

// Rebuilds the R/G/B/luma histograms and the clipping overlay from the image
// currently on screen (the mid-drag preview, or the full-resolution result).
// The counting and the overlay are histogram.hpp; this only picks the image
// and stores the results where the histogram and canvas painters read them.
static void computeHistogram() {
  const CanvasImage *img =
      state.showPreview() ? state.previewImage.get() : state.image.get();
  const histogram::Counts c = histogram::compute(img);
  state.histR = c.r;
  state.histG = c.g;
  state.histB = c.b;
  state.histL = c.l;
  state.histClipLo = c.clipLo;
  state.histClipHi = c.clipHi;
  state.clipOverlay = histogram::clipOverlay(
      img, {state.clipShadowsOn, state.clipHighlightsOn, state.clipPerChannel});
  state.scopeImage = scopes::render(state.scopeKind, img);
  state.histStale = false;
  ++state.histVersion;
}

// Luma as gray bars, R/G/B as thin colored outlines on top. Bar heights use
// sqrt scaling and are normalized to the tallest bin among 1..254, so a pile
// of clipped pixels in bin 0/255 doesn't flatten the rest of the graph.
static void paintHistogramBars(CanvasContext &ctx, float W, float H) {
  uint32_t peak = 1;
  for (int i = 1; i < 255; ++i)
    peak = std::max(
        {peak, state.histL[i], state.histR[i], state.histG[i], state.histB[i]});
  const float invPeak = 1.0f / std::sqrt(static_cast<float>(peak));
  const float binW = W / 256.0f;
  auto heightOf = [&](uint32_t v) {
    return std::min(1.0f, std::sqrt(static_cast<float>(v)) * invPeak) *
           (H - 2.0f);
  };

  ctx.setFillColor(Color{170, 170, 175, 255});
  for (int i = 0; i < 256; ++i) {
    const float h = heightOf(state.histL[i]);
    if (h > 0.0f)
      ctx.fillRect(static_cast<float>(i) * binW, H - h, binW + 0.5f, h);
  }

  auto outline = [&](const std::array<uint32_t, 256> &bins, Color c) {
    ctx.setStrokeColor(c);
    ctx.setLineWidth(1.0f);
    ctx.beginPath();
    for (int i = 0; i < 256; ++i) {
      const float x = (static_cast<float>(i) + 0.5f) * binW;
      const float y = H - heightOf(bins[i]);
      if (i == 0)
        ctx.moveTo(x, y);
      else
        ctx.lineTo(x, y);
    }
    ctx.stroke();
  };
  outline(state.histR, Color{235, 70, 70, 200});
  outline(state.histG, Color{70, 200, 90, 200});
  outline(state.histB, Color{80, 130, 255, 200});
}

// The two clipping triangles in the top corners of the pinned canvas. They
// stay in every view (histogram, waveform, vectorscope): clipping is a
// property of the picture, not of the way it is plotted.
static void paintClipTriangles(CanvasContext &ctx, float W) {
  // Clipping triangles. Dark: nothing clipped. Otherwise colored by which
  // channels clip (white = all three). White outline: that overlay is on.
  auto triangle = [&](bool left, uint8_t mask, bool overlayOn) {
    const float s = kHistTriPx, m = 3.0f;
    const Color c = mask ? histogram::clipChannelColor(mask, false)
                         : Color{90, 90, 95, 255};
    const float x0 = left ? m : W - m;
    const float dx = left ? s : -s;
    auto path = [&] {
      ctx.beginPath();
      ctx.moveTo(x0, m);
      ctx.lineTo(x0 + dx, m);
      ctx.lineTo(x0, m + s);
      ctx.closePath();
    };
    path();
    ctx.setFillColor(c);
    ctx.fill();
    if (overlayOn) {
      path();
      ctx.setStrokeColor(kColorWhite);
      ctx.setLineWidth(1.0f);
      ctx.stroke();
    }
  };
  triangle(true, state.histClipLo, state.clipShadowsOn);
  triangle(false, state.histClipHi, state.clipHighlightsOn);
}

// Waveform or vectorscope: the picture scopes::render made, stretched over the
// canvas (the waveform fills it, the vectorscope is a centered square), plus a
// faint graticule and the view's name in the bottom-left corner.
static void paintScope(CanvasContext &ctx, float W, float H) {
  const bool wave = state.scopeKind == scopes::Kind::Waveform;
  float x0 = 0.0f, y0 = 0.0f, w = W, h = H;
  if (!wave) {
    const float side = std::min(W, H);
    x0 = (W - side) / 2.0f;
    y0 = (H - side) / 2.0f;
    w = h = side;
  }
  if (state.scopeImage)
    ctx.drawImage(*state.scopeImage, x0, y0, w, h);

  ctx.setLineWidth(1.0f);
  ctx.setStrokeColor(Color{255, 255, 255, 40});
  ctx.beginPath();
  if (wave) {
    // Quarter lines of the 0..255 level axis.
    for (int q = 1; q <= 3; ++q) {
      const float y = std::floor(y0 + h * static_cast<float>(q) / 4.0f) + 0.5f;
      ctx.moveTo(0.0f, y);
      ctx.lineTo(W, y);
    }
  } else {
    // Outer ring (full saturation), half ring, the two axes, and the skin-tone
    // line (the I axis, 123 degrees from +Cb toward red).
    const float cx = x0 + w / 2.0f, cy = y0 + h / 2.0f, r = w / 2.0f - 1.0f;
    ctx.moveTo(cx + r, cy);
    ctx.arc(cx, cy, r, 0.0f, 6.2831853f);
    ctx.moveTo(cx + r / 2.0f, cy);
    ctx.arc(cx, cy, r / 2.0f, 0.0f, 6.2831853f);
    ctx.moveTo(cx - r, cy);
    ctx.lineTo(cx + r, cy);
    ctx.moveTo(cx, cy - r);
    ctx.lineTo(cx, cy + r);
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx - 0.545f * r, cy - 0.839f * r);
  }
  ctx.stroke();

  ctx.setFont("", 9.0f);
  ctx.setTextAlign(TextAlign::Start);
  ctx.setTextBaseline(TextBaseline::Top);
  ctx.setFillColor(Color{255, 255, 255, 110});
  ctx.fillText(scopes::kindName(state.scopeKind), 4.0f, H - 13.0f);
}

// The pinned canvas: the histogram, or the waveform / vectorscope picked with
// a click (cycleScope), always with the clipping triangles on top.
static void paintHistogram(CanvasContext &ctx) {
  const float W = ctx.width(), H = ctx.height();
  uiState.hist.canvasW = W;
  ctx.setFillColor(Color{34, 34, 38, 255});
  ctx.fillRect(0, 0, W, H);
  if (state.scopeKind == scopes::Kind::Histogram)
    paintHistogramBars(ctx, W, H);
  else
    paintScope(ctx, W, H);
  paintClipTriangles(ctx, W);
}

static void refreshHistogramIfStale() {
  flushPendingAdjust();
  if (state.histStale)
    computeHistogram();
}

// Turns the shadow / highlight overlays on or off. Recomputing right away
// rebuilds the overlay, bumps histVersion (so the histogram repaints its
// triangles) and the canvas is told to repaint.
static void setClipping(bool shadows, bool highlights) {
  state.clipShadowsOn = shadows;
  state.clipHighlightsOn = highlights;
  computeHistogram();
  state.markDirty();
}
// J: both on, or both off if either is currently on.
static void toggleClipping() {
  if (!state.image)
    return;
  const bool on = !(state.clipShadowsOn || state.clipHighlightsOn);
  setClipping(on, on);
}

// Shift+J: switch between classic red/blue and per-channel colors. If no
// overlay is on yet, turn both on so the key always shows something.
static void toggleChannelClipping() {
  if (!state.image)
    return;
  state.clipPerChannel = !state.clipPerChannel;
  if (!state.clipShadowsOn && !state.clipHighlightsOn)
    state.clipShadowsOn = state.clipHighlightsOn = true;
  computeHistogram(); // rebuild the overlay with the new coloring
  state.markDirty();
}

// A click on the pinned canvas (outside the clipping corners): Histogram ->
// Waveform -> Vectorscope -> Histogram. computeHistogram() rebuilds the scope
// picture for the new view and bumps histVersion, so the canvas repaints; its
// height follows scopeKind (see where the canvas is built).
static void cycleScope() {
  if (!state.image)
    return;
  state.scopeKind = scopes::nextKind(state.scopeKind);
  computeHistogram();
  state.markDirty();
}

