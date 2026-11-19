// app/mask_effects.hpp: mask effects: Amount, Color Mix, Grading, Curve
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Mask effects: Amount, Color Mix, Grading, Curve ----------
// The Effect tab of the right-hand mask panel. Everything here edits the
// SELECTED mask through maskEdit, so it is undoable and a drag folds into one
// step. The ids below are the "control" part of that undo key: they must stay
// below kMaskCtlStride and differ from the kMaskCtl* ids above.
constexpr int kMaskCtlAmount = 6;
constexpr int kMaskCtlMix = 30;   // + chan * kMixBandCount + band   (30..53)
constexpr int kMaskCtlGrade = 60; // + field * 4 + range             (60..71)
constexpr int kMaskCtlBlend = 72;
constexpr int kMaskCtlBalance = 73;
constexpr int kMaskCtlCurve = 80; // + channel                       (80..83)

// All control ids of one mask, as inclusive ranges. They must not overlap and
// must stay in 0..kMaskCtlStride-1, or two different controls would fold into
// one undo step. (Adj used to be 10+AdjId, which overlapped the Color Mixer
// ids.)
static_assert(
    [] {
      struct R {
        int lo, hi;
      };
      constexpr int kMixN = static_cast<int>(kMixChanCount * kMixBandCount);
      constexpr R r[] = {
          {kMaskCtlFeather, kMaskCtlFeather},
          {kMaskCtlAngle, kMaskCtlAngle},
          {kMaskCtlWidth, kMaskCtlWidth},
          {kMaskCtlHeight, kMaskCtlHeight},
          {kMaskCtlLength, kMaskCtlLength},
          {kMaskCtlAmount, kMaskCtlAmount},
          {kMaskCtlMix, kMaskCtlMix + kMixN - 1},
          {kMaskCtlGrade, kMaskCtlGrade + 11},
          {kMaskCtlBlend, kMaskCtlBlend},
          {kMaskCtlBalance, kMaskCtlBalance},
          {kMaskCtlCurve, kMaskCtlCurve + 3},
          {kMaskCtlRange, kMaskCtlRange},
          {kMaskCtlName, kMaskCtlName},
          {kMaskCtlMove, kMaskCtlMove},
          {kMaskCtlResizeX, kMaskCtlResizeX},
          {kMaskCtlResizeY, kMaskCtlResizeY},
          {kMaskCtlLinEnd, kMaskCtlLinEnd},
          {kMaskCtlLinStart, kMaskCtlLinStart},
          {kMaskCtlBrush, kMaskCtlBrush},
          {kMaskCtlLumLo, kMaskCtlLumLo},
          {kMaskCtlLumHi, kMaskCtlLumHi},
          {kMaskCtlPolyVertex, kMaskCtlPolyVertex},
          {kMaskCtlPolyMove, kMaskCtlPolyMove},
          {kMaskCtlPolyLasso, kMaskCtlPolyLasso},
          {kMaskCtlAdj, kMaskCtlAdj + static_cast<int>(AdjId::Count) - 1}};
      constexpr int n = static_cast<int>(sizeof(r) / sizeof(r[0]));
      for (int i = 0; i < n; ++i) {
        if (r[i].lo < 0 || r[i].hi >= kMaskCtlStride || r[i].lo > r[i].hi)
          return false;
        for (int j = i + 1; j < n; ++j)
          if (r[i].lo <= r[j].hi && r[j].lo <= r[i].hi)
            return false;
      }
      return true;
    }(),
    "mask control ids overlap or exceed kMaskCtlStride (see maskEdit)");
constexpr float kMaskCurveSizePx = 240.0f;


static std::string maskIntText(float v) {
  return std::to_string(static_cast<int>(std::lround(v)));
}
static std::string maskSignedText(float v) {
  const int i = static_cast<int>(std::lround(v));
  return i > 0 ? "+" + std::to_string(i) : std::to_string(i);
}

// Amount: how strongly all of this mask's adjustments apply (Lightroom's
// mask "Amount"). 100 = full, 0 = the mask does nothing.
static View buildMaskAmountSlider() {
  SliderSpec s;
  s.name = "Amount";
  s.lo = 0.0f;
  s.hi = 100.0f;
  s.trackW = kMaskTrackW;
  s.get = [] {
    const Mask *m = selectedMask();
    return m ? m->amount : 100.0f;
  };
  s.set = [](float v) {
    maskEdit(
        selMask(), [&](Mask &m) { m.amount = std::round(v); }, kMaskCtlAmount);
  };
  s.format = maskIntText;
  s.onEnd = [] { state.adjust.endMaskEdit(); };
  return buildSliderRow(s);
}

// One Color Mixer slider (a band's Hue / Saturation / Luminance) of the mask.
static View buildMaskMixSlider(MixChan chan, size_t band) {
  SliderSpec s;
  s.name = mixBandName(band);
  s.lo = static_cast<float>(kMixMin);
  s.hi = static_cast<float>(kMixMax);
  s.trackW = kMaskTrackW;
  s.get = [chan, band] {
    const Mask *m = selectedMask();
    return m ? m->mix[mixIndex(chan, band)] : 0.0f;
  };
  s.set = [chan, band](float v) {
    maskEdit(
        selMask(),
        [&](Mask &m) { m.mix[mixIndex(chan, band)] = std::round(v); },
        kMaskCtlMix + static_cast<int>(chan) * static_cast<int>(kMixBandCount) +
            static_cast<int>(band));
  };
  s.format = maskSignedText;
  s.hasAccent = true;
  s.accent = mixBandColor(band);
  s.onEnd = [] { state.adjust.endMaskEdit(); };
  return buildSliderRow(s);
}

// One hue / saturation / luminance slider of one grading range of the mask.
// fieldIdx (0 hue, 1 sat, 2 lum) only keeps the undo keys apart.
static View buildMaskGradeSlider(const char *name, int range,
                                 float GradeWheel::*field, int fieldIdx,
                                 float lo, float hi, bool isSigned) {
  SliderSpec s;
  s.name = name;
  s.lo = lo;
  s.hi = hi;
  s.trackW = kMaskTrackW;
  s.get = [range, field] {
    const Mask *m = selectedMask();
    return m ? m->grade.wheel[static_cast<size_t>(range)].*field : 0.0f;
  };
  s.set = [range, field, fieldIdx](float v) {
    maskEdit(
        selMask(),
        [&](Mask &m) {
          m.grade.wheel[static_cast<size_t>(range)].*field = std::round(v);
        },
        kMaskCtlGrade + fieldIdx * 4 + range);
  };
  s.format = [isSigned](float v) {
    return isSigned ? maskSignedText(v) : maskIntText(v);
  };
  s.onEnd = [] { state.adjust.endMaskEdit(); };
  return buildSliderRow(s);
}

// Blending (0..100) or Balance (-100..100), shared by the mask's ranges.
static View buildMaskGradeShared(bool balance) {
  SliderSpec s;
  s.name = balance ? "Balance" : "Blending";
  s.lo = balance ? -100.0f : 0.0f;
  s.hi = 100.0f;
  s.trackW = kMaskTrackW;
  s.get = [balance] {
    const Mask *m = selectedMask();
    if (!m)
      return 0.0f;
    return balance ? m->grade.balance : m->grade.blending;
  };
  s.set = [balance](float v) {
    maskEdit(
        selMask(),
        [&](Mask &m) {
          if (balance)
            m.grade.balance = std::round(v);
          else
            m.grade.blending = std::round(v);
        },
        balance ? kMaskCtlBalance : kMaskCtlBlend);
  };
  s.format = [balance](float v) {
    return balance ? maskSignedText(v) : maskIntText(v);
  };
  s.onEnd = [] { state.adjust.endMaskEdit(); };
  return buildSliderRow(s);
}

