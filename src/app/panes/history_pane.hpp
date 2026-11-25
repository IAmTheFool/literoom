// app/panes/history_pane.hpp: the History pane (buildHistoryPane)
#pragma once

#include <cstddef>    // size_t
#include <functional> // std::function
#include <string>     // std::string
#include <utility>    // std::move

#include "liteui.hpp"              // View, Text, Size, FlexDirection, Display, EdgeInsets

#include "app/constants.hpp"       // kHistoryActivity, kColorTextMuted
#include "app/editor_state.hpp"    // inline EditorState state
#include "app/history_panel.hpp"   // buildHistoryRow 
#include "edit/adjustment_set.hpp" // kMaxUndoSteps

// -- "History" pane: every undo step, newest first. Click to jump.
inline View buildHistoryPane() {
  View historyPane;
  historyPane.style.width = Size::full();
  historyPane.style.direction = FlexDirection::Column;
  historyPane.style.gap = 2.0f;
  historyPane.style.display = std::function<Display()>([] {
    return state.activeActivity == kHistoryActivity ? Display::Flex
                                                    : Display::None;
  });

  Text historyHint;
  historyHint.label = std::function<std::string()>([]() -> std::string {
    return state.image
               ? "Click a step to go back to it. Dimmed steps are ahead of "
                 "you: a new edit discards them."
               : "Open an image to see its edit history.";
  });
  historyHint.fontSize = 12.0f;
  historyHint.color = kColorTextMuted;
  historyHint.style.margin = EdgeInsets{0, 0, 6, 0};
  historyPane.addChild(std::move(historyHint));

  for (size_t r = 0; r < kMaxUndoSteps + 1; ++r)
    historyPane.addChild(buildHistoryRow(r));

  return historyPane;
}
