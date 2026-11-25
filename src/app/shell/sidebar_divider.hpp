// app/shell/sidebar_divider.hpp: the draggable divider right of the sidebar
#pragma once

#include <algorithm>  // std::clamp
#include <functional> // std::function

#include "liteui.hpp" // View, Size, Cursor

#include "app/constants.hpp"    // kColorDivider, kColorDividerHover, kDividerW,
                                // kNoActivity, kMinSidebarW, kMaxSidebarW
#include "app/editor_state.hpp" // inline EditorState state
#include "app/widgets.hpp"      // bindDrag

// ---- Vertical divider — drag to resize the sidebar ----
inline View buildSidebarDivider() {
  View vDivider;
  vDivider.style.height = Size::full();
  vDivider.style.flexShrink = 0;
  vDivider.style.backgroundColor = kColorDivider;
  vDivider.style.hoverColor = kColorDividerHover;
  vDivider.style.width = std::function<Size()>([] {
    return Size::pixel(state.activeActivity != kNoActivity ? kDividerW : 0.0f);
  });

  auto updateSidebarWidth = [](float localX) {
    float next = state.sidebarWidth + localX - kDividerW / 2.0f;
    state.sidebarWidth = std::clamp(next, kMinSidebarW, kMaxSidebarW);
  };
  {
    auto resize = [updateSidebarWidth](float localX, float) {
      updateSidebarWidth(localX);
    };
    bindDrag(vDivider, resize, resize, nullptr, nullptr, Cursor::ResizeEW);
  }

  return vDivider;
}
