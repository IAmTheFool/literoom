// app/panes/pane_helpers.hpp: small views shared by several panes
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// A small muted heading above a group of controls (Adjust sections, mask
// shape sections).
inline Text sectionLabel(const char *text) {
  Text h;
  h.label = std::string(text);
  h.fontSize = 12.0f;
  h.color = kColorTextMuted;
  return h;
}
