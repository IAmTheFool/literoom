// app/shell/mask_panel.hpp: the right-hand mask panel and its divider (addMaskPanel)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// ---- Right-hand panel: the selected mask's controls ----
// Only on the Masks pane with a mask selected, so every other pane keeps
// the full canvas width. Drag its divider to resize.
static void addMaskPanel(View &workspace, View maskEditor) {
  const auto maskPanelShown = [] { return maskToolActive() && selMask() >= 0; };

  View rDivider;
  rDivider.style.height = Size::full();
  rDivider.style.flexShrink = 0;
  rDivider.style.backgroundColor = kColorDivider;
  rDivider.style.hoverColor = kColorDividerHover;
  rDivider.style.width = Size::pixel(kDividerW);
  rDivider.style.display =
      std::function<Display()>([maskPanelShown]() -> Display {
        return maskPanelShown() ? Display::Flex : Display::None;
      });
  const auto updateMaskPanelWidth = [](float localX) {
    // The panel sits to the right, so it grows as the divider moves LEFT.
    const float next = state.maskPanelW - (localX - kDividerW / 2.0f);
    state.maskPanelW = std::clamp(next, kMinMaskPanelW, kMaxMaskPanelW);
  };
  {
    auto resize = [updateMaskPanelWidth](float localX, float) {
      updateMaskPanelWidth(localX);
    };
    bindDrag(rDivider, resize, resize, nullptr, nullptr, Cursor::ResizeEW);
  }
  workspace.addChild(std::move(rDivider));

  View maskPanel;
  maskPanel.style.height = Size::full();
  maskPanel.style.flexShrink = 0;
  maskPanel.style.direction = FlexDirection::Column;
  maskPanel.style.backgroundColor = kColorPanelBg;
  maskPanel.style.padding = EdgeInsets::all(kMaskPanelPad);
  maskPanel.style.overflowX = Overflow::Hidden;
  maskPanel.style.overflowY = Overflow::Visible;
  maskPanel.style.width =
      std::function<Size()>([] { return Size::pixel(state.maskPanelW); });
  maskPanel.style.display =
      std::function<Display()>([maskPanelShown]() -> Display {
        return maskPanelShown() ? Display::Flex : Display::None;
      });

  Text maskPanelTitle;
  maskPanelTitle.label = std::function<std::string()>([]() -> std::string {
    const Mask *m = selectedMask();
    if (!m)
      return std::string();
    return "MASK " + std::to_string(selMask() + 1) + "  " + maskLabel(*m);
  });
  maskPanelTitle.fontSize = 12.0f;
  maskPanelTitle.fontWeight = FontWeight::SemiBold;
  maskPanelTitle.color = kColorTextMuted;
  maskPanelTitle.style.margin = EdgeInsets{0, 0, 10, 0};
  maskPanelTitle.style.flexShrink = 0;
  maskPanel.addChild(std::move(maskPanelTitle));

  // Scrolling body, like the left sidebar's.
  View maskPanelBody;
  maskPanelBody.style.width = Size::full();
  maskPanelBody.style.flexGrow = 1;
  maskPanelBody.style.minHeight = 0;
  maskPanelBody.style.direction = FlexDirection::Column;
  maskPanelBody.style.overflowX = Overflow::Hidden;
  maskPanelBody.style.overflowY = Overflow::Auto;
  maskPanelBody.addChild(std::move(maskEditor));
  maskPanel.addChild(std::move(maskPanelBody));
  workspace.addChild(std::move(maskPanel));
}
