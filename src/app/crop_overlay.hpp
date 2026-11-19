// app/crop_overlay.hpp: paints the crop selection over the picture
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

static void paintCropOverlay(CanvasContext &ctx) {
  const auto &c = state.crop;
  const float W = state.docWidth(), H = state.docHeight();
  const float px = 1.0f / state.zoom; // one screen pixel, in image units
  const float sw = c.r - c.l, sh = c.b - c.t;

  ctx.setFillColor(Color{0, 0, 0, 120});
  ctx.fillRect(0, 0, W, c.t);          // above
  ctx.fillRect(0, c.b, W, H - c.b);    // below
  ctx.fillRect(0, c.t, c.l, sh);       // left
  ctx.fillRect(c.r, c.t, W - c.r, sh); // right

  ctx.setLineWidth(px);
  ctx.setStrokeColor(Color{255, 255, 255, 110});
  ctx.beginPath();
  for (int i = 1; i <= 2; ++i) {
    const float x = c.l + sw * static_cast<float>(i) / 3.0f;
    const float y = c.t + sh * static_cast<float>(i) / 3.0f;
    ctx.moveTo(x, c.t);
    ctx.lineTo(x, c.b);
    ctx.moveTo(c.l, y);
    ctx.lineTo(c.r, y);
  }
  ctx.stroke();

  ctx.setStrokeColor(kColorWhite);
  ctx.setLineWidth(px * 1.5f);
  ctx.strokeRect(c.l, c.t, sw, sh);

  const float hs = kCropHandlePx * px;
  const bool midX = sw > hs * 4.0f, midY = sh > hs * 4.0f;
  const float xs[3] = {c.l, (c.l + c.r) / 2.0f, c.r};
  const float ys[3] = {c.t, (c.t + c.b) / 2.0f, c.b};
  ctx.setFillColor(kColorWhite);
  ctx.setStrokeColor(Color{60, 60, 60, 255});
  ctx.setLineWidth(px);
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      if (i == 1 && j == 1)
        continue; // center: no handle
      if ((i == 1 && !midX) || (j == 1 && !midY))
        continue; // edge midpoints crowd together on tiny selections
      ctx.fillRect(xs[i] - hs / 2.0f, ys[j] - hs / 2.0f, hs, hs);
      ctx.strokeRect(xs[i] - hs / 2.0f, ys[j] - hs / 2.0f, hs, hs);
    }
}

