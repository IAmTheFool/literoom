// app/color_ui.hpp: Color Mixer and Color Grading UI
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Color Mixer UI ----------------

// One Color Mixer slider (a band's Hue, Saturation or Luminance), tinted in
// the band's own color. Same undo/preview behavior as every Adjust slider.
static View buildMixSlider(MixChan chan, size_t band) {
  SliderSpec s;
  s.name = mixBandName(band);
  s.lo = static_cast<float>(kMixMin);
  s.hi = static_cast<float>(kMixMax);
  s.get = [chan, band] { return state.adjust.mix(chan, band); };
  s.set = [chan, band](float v) { state.adjust.setMix(chan, band, v); };
  s.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v)));
  };
  s.hasAccent = true;
  s.accent = mixBandColor(band);
  return buildSliderRow(s);
}

// Zeroes all 24 mixer sliders (one undo step).
static void resetColorMix() {
  state.adjust.resetMix();
  state.markAdjustDirty();
}

// ---------------- Color Grading UI ----------------
constexpr float kGradePaneW =
    250.0f; // sidebar width the wheels are laid out for
constexpr float kGradePi = 3.14159265358979f;
constexpr int kGradeMainWheel = 130;   // Midtones wheel in 3-way mode
constexpr int kGradeSmallWheel = 84;   // Shadows / Highlights in 3-way mode
constexpr int kGradeSingleWheel = 170; // wheel in the single-range modes
constexpr float kGradeTabW = 40.0f;


static const char *gradeRangeName(int r) {
  static const char *const n[kGradeRangeCount] = {"Shadows", "Midtones",
                                                  "Highlights", "Global"};
  return n[r];
}

// Wheel brightness is cosmetic (Lightroom dims the shadow wheel).
constexpr float kGradeWheelBrightness[kGradeRangeCount] = {0.62f, 1.0f, 0.80f,
                                                           1.0f};

static Color gradeHsv(float h, float s, float v, uint8_t a = 255) {
  float c[3];
  gradeHueToRgb(h, c);
  auto to8 = [&](float f) {
    const float o = (f * s + (1.0f - s)) * v * 255.0f; // blend toward white
    return static_cast<uint8_t>(std::lround(std::clamp(o, 0.0f, 255.0f)));
  };
  return Color{to8(c[0]), to8(c[1]), to8(c[2]), a};
}

static size_t gradeHash(float v) { return std::hash<float>{}(v); }
static size_t gradeMix(size_t a, size_t b) {
  return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
}

// Pre-renders the hue/saturation disc once per (diameter, brightness):
// angle = hue (0 at 3 o'clock, counter-clockwise), radius = saturation,
// white in the centre. Cached in a map so the CanvasImage address stays
// stable (liteui caches the converted surface per image).
static const CanvasImage &gradeWheelImage(int d, float brightness) {
  static std::map<std::pair<int, int>, CanvasImage> cache;
  const auto key =
      std::make_pair(d, static_cast<int>(std::lround(brightness * 100)));
  const auto it = cache.find(key);
  if (it != cache.end())
    return it->second;

  CanvasImage img(d, d);
  const float R = d / 2.0f;
  for (int y = 0; y < d; ++y) {
    for (int x = 0; x < d; ++x) {
      const float dx = x + 0.5f - R, dy = y + 0.5f - R;
      const float dist = std::sqrt(dx * dx + dy * dy);
      const float alpha = std::clamp(R - dist + 0.5f, 0.0f, 1.0f); // AA rim
      if (alpha <= 0.0f)
        continue;
      const float hue = std::atan2(-dy, dx) * 180.0f / kGradePi;
      const Color c = gradeHsv(hue, std::min(1.0f, dist / R), brightness);
      const size_t i = (static_cast<size_t>(y) * d + x) * 4;
      img.pixels[i + 0] = c.r;
      img.pixels[i + 1] = c.g;
      img.pixels[i + 2] = c.b;
      img.pixels[i + 3] = static_cast<uint8_t>(std::lround(alpha * 255.0f));
    }
  }
  return cache.emplace(key, std::move(img)).first->second;
}

// Applies `fn` to a copy of the current grade and commits it (key: see
// AdjustmentSet::setGrade).
static void gradeEdit(const std::function<void(ColorGrade &)> &fn, int key) {
  ColorGrade g = state.adjust.grade();
  fn(g);
  state.adjust.setGrade(g, key);
}

// Leaves the low-res drag preview and asks for one full-resolution render.
static void gradeEndDrag() {
  state.adjust.endGradeEdit();
  if (state.dragging) {
    state.dragging = false;
    state.dragIsAngle = false;
    state.markAdjustDirty();
  }
}

// A slider bound to one field (hue / sat / lum) of one wheel.
static View buildGradeWheelSlider(const std::string &name, int range,
                                  float GradeWheel::*field, float lo, float hi,
                                  int keyBase, float trackW, bool isSigned) {
  SliderSpec s;
  s.name = name;
  s.lo = lo;
  s.hi = hi;
  s.trackW = trackW;
  s.get = [range, field] { return state.adjust.grade().wheel[range].*field; };
  s.set = [range, field, keyBase](float v) {
    gradeEdit([&](ColorGrade &g) { g.wheel[range].*field = std::round(v); },
              keyBase + range);
    uiState.grade.active = range;
  };
  s.format = [isSigned](float v) {
    const int i = static_cast<int>(std::lround(v));
    return isSigned && i > 0 ? "+" + std::to_string(i) : std::to_string(i);
  };
  s.onEnd = [] { state.adjust.endGradeEdit(); };
  return buildSliderRow(s);
}

// One hue/saturation wheel: click or drag sets hue (angle) + saturation
// (radius); right-click resets both.
static View buildGradeWheel(int range, int diameter, float pad) {
  const float size = static_cast<float>(diameter) + 2.0f * pad;
  View c;
  c.isCanvas = true;
  c.style.width = Size::pixel(size);
  c.style.height = Size::pixel(size);
  c.style.flexShrink = 0;

  auto apply = [range, diameter, size](float lx, float ly, bool drag) {
    if (!state.original)
      return; // no image loaded yet — nothing to grade
    const float R = diameter / 2.0f, cx = size / 2.0f, cy = size / 2.0f;
    const float dx = lx - cx, dy = ly - cy;
    const float dist = std::sqrt(dx * dx + dy * dy);
    gradeEdit(
        [&](ColorGrade &g) {
          GradeWheel &w = g.wheel[range];
          w.sat = std::round(std::clamp(dist / R, 0.0f, 1.0f) * 100.0f);
          if (dist > 2.0f) { // keep the old hue while sitting on the centre
            float hue = std::atan2(-dy, dx) * 180.0f / kGradePi;
            if (hue < 0.0f)
              hue += 360.0f;
            w.hue = std::fmod(std::round(hue), 360.0f);
          }
        },
        range);
    uiState.grade.active = range;
    if (drag)
      enterDrag(false); // switches recompute+paint to the preview path
    state.markAdjustDirty();
  };
  bindDrag(
      c, [apply](float lx, float ly) { apply(lx, ly, false); },
      [apply](float lx, float ly) { apply(lx, ly, true); },
      [] { gradeEndDrag(); }, nullptr, Cursor::Crosshair);
  c.onContextMenu =
      [range](PointerEvent &e) { // reset hue + sat, keep luminance
        e.stopPropagation();
        if (!state.original)
          return;
        gradeEdit(
            [&](ColorGrade &g) {
              g.wheel[range].hue = 0.0f;
              g.wheel[range].sat = 0.0f;
            },
            -1);
        uiState.grade.active = range;
        state.markAdjustDirty();
      };

  c.onPaint = [range, diameter](CanvasContext &ctx) {
    const GradeWheel &w = state.adjust.grade().wheel[range];
    const float cx = ctx.width() / 2, cy = ctx.height() / 2;
    const float R = diameter / 2.0f;

    ctx.setGlobalAlpha(w.visible ? 1.0f : 0.3f);
    // The disc is a bitmap: build it at device resolution and draw it at its
    // logical size, so it is as sharp as the vector drawing around it.
    const int devDiameter = std::max(
        1, static_cast<int>(std::lround(diameter * ctx.devicePixelRatio())));
    ctx.drawImage(gradeWheelImage(devDiameter, kGradeWheelBrightness[range]),
                  cx - R, cy - R, static_cast<float>(diameter),
                  static_cast<float>(diameter));
    ctx.setGlobalAlpha(1.0f);

    const float ang = -w.hue * kGradePi / 180.0f;
    const float ca = std::cos(ang), sa = std::sin(ang);
    const float dist = w.sat / 100.0f * R;
    const float px = cx + ca * dist, py = cy + sa * dist;

    // spoke from the centre to the puck
    ctx.setStrokeColor(gradeHsv(w.hue, 0.30f, 1.0f, 200));
    ctx.setLineWidth(2.0f);
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(px, py);
    ctx.stroke();

    // hue dot outside the rim (grey while saturation is 0)
    const float k = std::clamp(w.sat / 100.0f * 3.0f, 0.0f, 1.0f);
    const Color full = gradeHsv(w.hue, 1.0f, 1.0f);
    const Color grey{70, 70, 70};
    auto lerp8 = [k](uint8_t a, uint8_t b) {
      return static_cast<uint8_t>(std::lround(a + (b - a) * k));
    };
    ctx.setFillColor(Color{lerp8(grey.r, full.r), lerp8(grey.g, full.g),
                           lerp8(grey.b, full.b)});
    ctx.beginPath();
    ctx.arc(cx + ca * (R + 11), cy + sa * (R + 11), 5.0f, 0, 2 * kGradePi);
    ctx.fill();

    // puck: dark ring with a faint light halo so it reads on any colour
    ctx.setStrokeColor(Color{255, 255, 255, 90});
    ctx.setLineWidth(1.0f);
    ctx.beginPath();
    ctx.arc(px, py, 9.0f, 0, 2 * kGradePi);
    ctx.stroke();
    ctx.setStrokeColor(Color{50, 50, 50});
    ctx.setLineWidth(2.0f);
    ctx.beginPath();
    ctx.arc(px, py, 7.5f, 0, 2 * kGradePi);
    ctx.stroke();
  };

  // Repaint only when this wheel changed (drag, undo/redo, reset, new image).
  auto last = std::make_shared<size_t>(~size_t{0});
  c.canvasDirtySource = [range, last]() -> bool {
    const GradeWheel &w = state.adjust.grade().wheel[range];
    size_t h = gradeHash(w.hue);
    h = gradeMix(h, gradeHash(w.sat));
    h = gradeMix(h, w.visible ? 1u : 0u);
    if (h == *last)
      return false;
    *last = h;
    return true;
  };
  return c;
}

// Eye icon: toggles that wheel's preview (one undo step).
static View buildGradeEye(int range) {
  View c;
  c.isCanvas = true;
  c.style.width = Size::pixel(22);
  c.style.height = Size::pixel(14);
  c.style.flexShrink = 0;
  c.onClick = [range] {
    if (!state.original)
      return;
    gradeEdit(
        [&](ColorGrade &g) {
          g.wheel[range].visible = !g.wheel[range].visible;
        },
        -1);
    state.markAdjustDirty();
  };
  c.onPaint = [range](CanvasContext &ctx) {
    const bool on = state.adjust.grade().wheel[range].visible;
    const float cx = ctx.width() / 2, cy = ctx.height() / 2;
    const Color col = on ? Color{190, 190, 190} : Color{118, 118, 118};
    ctx.setStrokeColor(col);
    ctx.setFillColor(col);
    ctx.setLineWidth(1.4f);
    ctx.beginPath();
    ctx.moveTo(cx - 8, cy);
    ctx.quadraticCurveTo(cx, cy - 9, cx + 8, cy);
    ctx.quadraticCurveTo(cx, cy + 9, cx - 8, cy);
    ctx.stroke();
    ctx.beginPath();
    ctx.arc(cx, cy, 2.2f, 0, 2 * kGradePi);
    ctx.fill();
    if (!on) {
      ctx.beginPath();
      ctx.moveTo(cx - 8, cy + 6);
      ctx.lineTo(cx + 8, cy - 6);
      ctx.stroke();
    }
  };
  auto last = std::make_shared<int>(-1);
  c.canvasDirtySource = [range, last]() -> bool {
    const int now = state.adjust.grade().wheel[range].visible ? 1 : 0;
    if (now == *last)
      return false;
    *last = now;
    return true;
  };
  return c;
}

// wheel + eye row + luminance slider, as one column
static View buildGradeWheelBlock(int range, int diameter, float pad) {
  const float size = static_cast<float>(diameter) + 2.0f * pad;
  View col;
  col.style.direction = FlexDirection::Column;
  col.style.alignItems = Align::Center;
  col.style.gap = 2.0f;
  col.addChild(buildGradeWheel(range, diameter, pad));

  View eyeRow;
  eyeRow.style.direction = FlexDirection::Row;
  eyeRow.style.justifyContent = Justify::End;
  eyeRow.style.width = Size::pixel(static_cast<float>(diameter));
  eyeRow.addChild(buildGradeEye(range));
  col.addChild(std::move(eyeRow));

  col.addChild(buildGradeWheelSlider("Lum", range, &GradeWheel::lum, -100, 100,
                                     4, std::min(150.0f, size - 8.0f), true));
  return col;
}

// 3-way mode: big Midtones wheel on top, Shadows + Highlights below.
static View buildGradeThreeWay() {
  View col;
  col.style.direction = FlexDirection::Column;
  col.style.alignItems = Align::Center;
  col.style.gap = 6.0f;
  col.addChild(buildGradeWheelBlock(static_cast<int>(GradeRange::Midtones),
                                    kGradeMainWheel, 20.0f));

  View row;
  row.style.direction = FlexDirection::Row;
  row.style.justifyContent = Justify::Center;
  row.style.gap = 2.0f;
  auto labeled = [](int range) {
    View v;
    v.style.direction = FlexDirection::Column;
    v.style.alignItems = Align::Center;
    Text t;
    t.label = gradeRangeName(range);
    t.fontSize = 12.0f;
    t.color = kColorTextMuted;
    v.addChild(std::move(t));
    v.addChild(buildGradeWheelBlock(range, kGradeSmallWheel, 14.0f));
    return v;
  };
  row.addChild(labeled(static_cast<int>(GradeRange::Shadows)));
  row.addChild(labeled(static_cast<int>(GradeRange::Highlights)));
  col.addChild(std::move(row));
  return col;
}

// Single-range mode: one large wheel + Hue / Saturation / Luminance sliders.
static View buildGradeSingle(int range) {
  View col;
  col.style.direction = FlexDirection::Column;
  col.style.alignItems = Align::Center;
  col.style.gap = 8.0f;
  col.addChild(buildGradeWheel(range, kGradeSingleWheel, 22.0f));
  col.addChild(buildGradeWheelSlider("Hue", range, &GradeWheel::hue, 0, 359, 8,
                                     150.0f, false));
  col.addChild(buildGradeWheelSlider("Saturation", range, &GradeWheel::sat, 0,
                                     100, 12, 150.0f, false));
  col.addChild(buildGradeWheelSlider("Luminance", range, &GradeWheel::lum, -100,
                                     100, 4, 150.0f, true));
  return col;
}

// Resets every wheel, Blending and Balance (one undo step).
static void resetColorGrade() {
  state.adjust.setGrade(ColorGrade{}, -1);
  state.markAdjustDirty();
}

// Draws the crop selection over the image: dimmed surround, rule-of-thirds
// guides, border, and eight drag handles. Expects the CTM to already be
// scaled by state.zoom (so everything here is in image pixels), and divides
// line widths / handle sizes by the zoom to keep them constant on screen.
