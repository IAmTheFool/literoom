// app/panes/pane_helpers.hpp: small views shared by several panes
#pragma once

#include <string> // std::string

#include "liteui.hpp"        // Text

#include "app/constants.hpp" // kColorTextMuted

// A small muted heading above a group of controls (Adjust sections, mask
// shape sections).
inline Text sectionLabel(const char *text) {
  Text h;
  h.label = std::string(text);
  h.fontSize = 12.0f;
  h.color = kColorTextMuted;
  return h;
}
