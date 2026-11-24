// app/canvas_paint.hpp: clipping overlay, GPU copies of the picture, paintCanvas
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// Red/blue clipping warning over the edited picture. The overlay image is
// in document space (stretched to docW x docH like the preview), so it
// lines up at any zoom. computeHistogram builds it.
inline void computeHistogram();
inline void paintClipOverlay(CanvasContext &ctx, float docW, float docH) {
  if (!state.clipShadowsOn && !state.clipHighlightsOn)
    return;
  if (state.histStale)
    computeHistogram(); // also rebuilds the overlay
  if (state.clipOverlay)
    ctx.drawImage(*state.clipOverlay, 0, 0, docW, docH);
}

// ---- The picture's GPU copies ----
// The main canvas draws up to three pictures: the edited image, the original
// ("source", for Before and Split) and the mid-drag preview. drawImage keeps
// ONE cached GPU texture per canvas, so alternating between them (Split
// compare, or a drag ending and starting again) re-uploaded the whole picture
// each time. Each one now has a CanvasTexture of its own, made lazily here the
// first time it is painted and replaced when the picture is.
struct PictureTex {
  std::shared_ptr<const CanvasImage> of; // the picture `tex` shows (held, so
                                         // its address cannot be reused)
  std::shared_ptr<CanvasTexture> tex;
};
inline PictureTex gImageTex, gSourceTex, gPreviewTex;

// The image and the source are built once and replaced, never edited in place
// (the editor's rule, see architecture.md), so the texture can SHARE their
// pixels: wrap() costs no extra memory. Null if there is no picture or it can't
// be wrapped; the caller then falls back to drawImage.
inline const std::shared_ptr<CanvasTexture> &
pictureTexture(PictureTex &slot, const std::shared_ptr<CanvasImage> &img) {
  if (slot.of != img) {
    slot.of = img;
    slot.tex = img ? CanvasTexture::wrap(img) : nullptr;
    if (slot.tex && !slot.tex->valid())
      slot.tex.reset();
  }
  return slot.tex;
}

// The drag preview is a NEW small image every frame (about 1 MP), so wrapping
// each would still create and delete a GPU texture per frame. One texture of
// the preview's size is kept instead and each new preview is copied into it
// (a few MB), which uploads as a sub-image into the same GPU texture.
inline const std::shared_ptr<CanvasTexture> &
previewTexture(const std::shared_ptr<CanvasImage> &img) {
  PictureTex &slot = gPreviewTex;
  if (slot.of != img) {
    slot.of = img;
    if (!img || img->width <= 0 || img->height <= 0) {
      slot.tex.reset();
    } else {
      if (!slot.tex || slot.tex->width() != img->width ||
          slot.tex->height() != img->height)
        slot.tex = std::make_shared<CanvasTexture>(img->width, img->height);
      if (!slot.tex->update(0, 0, *img))
        slot.tex.reset();
    }
  }
  return slot.tex;
}

inline void paintCanvas(CanvasContext &ctx) {
  LITEUI_PROF_START(profAppPaint);
  if (!state.image || state.image->width <= 0 || state.image->height <= 0) {
    gImageTex = PictureTex{}; // no picture: let go of the GPU copies
    gSourceTex = PictureTex{};
    gPreviewTex = PictureTex{};
    ctx.setFillColor(kColorCanvasPlaceholderBg);
    ctx.fillRect(0, 0, ctx.width(), ctx.height());
    return;
  }
  // The canvas is the whole viewport; the picture is a W x H rectangle inside
  // it at (ox, oy), on a whole device pixel. Everything picture-related below
  // is drawn into that rectangle (clipped to it, as the canvas used to do by
  // being exactly that size), in the same coordinates as before.
  ctx.setFillColor(kColorViewportBg);
  ctx.fillRect(0, 0, ctx.width(), ctx.height());
  const ViewGeo vg = viewGeo();
  float ox, oy;
  viewOrigin(vg, ox, oy);
  ctx.setFillColor(kColorWhite);
  ctx.fillRect(ox, oy, vg.W, vg.H);
  // A straightened image with "Keep full canvas" has transparent corners;
  // a neutral backdrop keeps them from reading as part of a white picture.
  if (state.adjust.expand() && state.adjust.angle() != 0.0f) {
    ctx.setFillColor(kColorEmptyCorner);
    ctx.fillRect(ox, oy, vg.W, vg.H);
  }

  ctx.save();
  ctx.beginPath();
  ctx.rect(ox, oy, vg.W, vg.H);
  ctx.clip();
  ctx.translate(ox, oy);
  ctx.save();
  ctx.scale(state.zoom, state.zoom);
  const float docW = state.docWidth(), docH = state.docHeight();
  // The picture as edited.
  const auto drawAfter = [&]() {
    if (state.showPreview()) {
      // Mid-drag (or awaiting the background full-res render) preview: smaller
      // image stretched to the full document size
      // (bilinear), so layout/zoom are unaffected by its lower resolution.
      if (const auto &t = previewTexture(state.previewImage))
        ctx.drawTexture(t, 0, 0, docW, docH);
      else
        ctx.drawImage(*state.previewImage, 0, 0, docW, docH);
    } else {
      if (const auto &t = pictureTexture(gImageTex, state.image))
        ctx.drawTexture(t, 0, 0);
      else
        ctx.drawImage(*state.image, 0, 0);
    }
  };
  // The same picture with every edit off (crop and rotation kept).
  const auto drawBefore = [&]() {
    if (state.source) {
      if (const auto &t = pictureTexture(gSourceTex, state.source))
        ctx.drawTexture(t, 0, 0, docW, docH);
      else
        ctx.drawImage(*state.source, 0, 0, docW, docH);
    } else {
      drawAfter();
    }
  };
  if (state.compare == CompareMode::Before) {
    drawBefore();
  } else if (state.compare == CompareMode::Split) {
    const float x = docW * state.splitPos;
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, 0, x, docH);
    ctx.clip();
    drawBefore();
    ctx.restore();
    ctx.save();
    ctx.beginPath();
    ctx.rect(x, 0, docW - x, docH);
    ctx.clip();
    drawAfter();
    ctx.restore();
  } else {
    drawAfter();
    paintClipOverlay(ctx, docW, docH);
    if (cropToolActive())
      paintCropOverlay(ctx);
    else if (maskToolActive())
      paintMaskOverlay(ctx);
  }
  ctx.restore();
  if (state.compare != CompareMode::Off)
    paintCompareOverlay(ctx); // labels and the split line (screen space)
  ctx.restore(); // the picture clip and offset
  // A hairline (one device pixel) just outside the picture.
  const float hair = 1.0f / state.dpr;
  ctx.setStrokeColor(kColorCanvasBorder);
  ctx.setLineWidth(hair);
  ctx.beginPath();
  ctx.rect(ox - hair / 2, oy - hair / 2, vg.W + hair, vg.H + hair);
  ctx.stroke();
  paintViewBars(ctx, vg);
  LITEUI_PROF_ADD(appPaint, profAppPaint);
}