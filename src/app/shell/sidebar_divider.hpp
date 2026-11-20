// app/shell/sidebar_divider.hpp: the draggable divider right of the sidebar (buildSidebarDivider)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// ---- Vertical divider — drag to resize the sidebar ----
static View buildSidebarDivider() {
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
