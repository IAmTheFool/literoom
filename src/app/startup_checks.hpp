// app/startup_checks.hpp: verifyAdjustmentSpecs (runs once at startup)
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// The pipeline indexes kAdjustmentSpecs by AdjId (the tone tables, the
// sidebar loop, the color pass), so entry i must describe AdjId i. This can't
// be a static_assert: the table holds std::function and std::string members,
// which aren't usable in constant expressions. So it's checked once at
// startup instead, and a mistake aborts with a message naming the bad entry
// rather than silently moving the wrong slider.
inline void verifyAdjustmentSpecs() {
  for (size_t i = 0; i < kAdjustmentSpecs.size(); ++i) {
    const AdjustmentSpec &s = kAdjustmentSpecs[i];
    const char *problem = nullptr;
    if (static_cast<size_t>(s.id) != i)
      problem = "is out of order: its id doesn't match its position in AdjId "
                "(or an entry is missing)";
    else if (!(s.min < s.max))
      problem = "has min >= max";
    else if (s.stage == AdjustmentSpec::Stage::Tone && !s.apply)
      problem = "is a Tone adjustment with no apply function";
    if (problem) {
      std::fprintf(stderr, "literoom: kAdjustmentSpecs[%zu] (\"%s\") %s\n", i,
                   std::string(s.label).c_str(), problem);
      std::abort();
    }
  }
}

