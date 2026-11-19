// app/widgets.hpp: small reusable widgets, pointer helpers, text-field keys
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Small reusable pieces ----------------

// ---- Pointer-event helpers
// --------------------------------------------------- Dispatch recap
// (liteui.hpp, "Pointer events"): a press runs the onPointerDown of EVERY view
// in its chain, deepest first, until one calls stopPropagation(). The chain
// holds each view under the pointer that has onPointerDown OR a click handler,
// so a view with only onClick does not stop a press from reaching an ancestor's
// onPointerDown.

// Closes the open titlebar dropdown. Presses that stopPropagation() never reach
// the workspace's own "close the menu" handler, so they call this themselves.
static void closeMenus() { state.openMenu = -1; }

// A press on this view belongs to it alone. Used on titlebar widgets: without
// it the press would bubble to the titlebar's onPointerDown, which closes menus
// and calls requestMove() (a window drag that swallows the release, so the
// click never arrives).
static void swallowPress(View &v) {
  v.onPointerDown = [](PointerEvent &e) { e.stopPropagation(); };
}

// ---- Keys for a focused text field ----
// A global shortcut is matched BEFORE the focused view's onKeyDown and a match
// ends the dispatch (LiteUIImpl::dispatchKeyDown). So a plain-key shortcut
// (Left, Right, Delete, Enter) would starve a focused TextInput of that key
// even when the shortcut itself stands down (see notTyping). The name fields
// are registered here so those shortcuts can hand the key to the field instead;
// it runs the very onKeyDown the TextInput was built with (same shared state).
struct TextFieldKeys {
  std::shared_ptr<TextInputState> state;
  std::function<void(KeyEvent)> onKeyDown;
};
static std::vector<TextFieldKeys> gTextFields;

// Call right after `parent.addChild(std::move(textInput))`, with that input's
// state and the view it flattened into (parent.children.back()).
static void registerTextField(const std::shared_ptr<TextInputState> &st,
                              const View &flattened) {
  gTextFields.push_back({st, flattened.onKeyDown});
}

// True if a registered field has focus and took the key.
static bool forwardKeyToTextField(const KeyEvent &e) {
  for (const TextFieldKeys &f : gTextFields)
    if (f.state && f.state->focused && f.onKeyDown) {
      f.onKeyDown(e);
      return true;
    }
  return false;
}

// A left-button drag gesture on one view, built on pointer events. press / drag
// / end get the view-local point. The press captures the pointer, so drag and
// release keep arriving even when the pointer leaves the view. `rightPress`
// (optional) handles right-button presses. `hover` / `dragging` (optional) are
// the cursors shown over the view and while it is being dragged
// (Style::cursor).
//
// The gesture belongs to ONE pointer (`owner`): touch can put several fingers
// on the same view, and only the first drives the drag; the others are ignored
// until it ends. For the mouse the owner is always pointer 1, so nothing
// changes.
static void bindDrag(View &v, std::function<void(float, float)> press,
                     std::function<void(float, float)> drag,
                     std::function<void()> end,
                     std::function<void(float, float)> rightPress = nullptr,
                     Cursor hover = Cursor::Inherit,
                     Cursor dragging = Cursor::Inherit) {
  auto owner = std::make_shared<std::optional<uint32_t>>();
  v.onPointerDown = [owner, press, rightPress](PointerEvent &e) {
    e.stopPropagation(); // this view owns the press (no ancestor handlers)
    if (e.button == MouseButton::Left) {
      closeMenus(); // ...so the workspace's close-the-menu handler won't see it
      if (*owner)
        return; // another pointer is already dragging this view
      *owner = e.pointerId;
      if (press)
        press(e.x, e.y);
    } else if (e.button == MouseButton::Right && rightPress)
      rightPress(e.x, e.y);
  };
  v.onPointerMove = [owner, drag](PointerEvent &e) {
    if (*owner == e.pointerId && drag && e.held(MouseButton::Left))
      drag(e.x, e.y);
  };
  auto finish = [owner, end](const PointerEvent &e) {
    if (*owner != e.pointerId)
      return; // not the pointer that owns the gesture
    owner->reset();
    if (end)
      end();
  };
  v.onPointerUp = [finish](PointerEvent &e) {
    if (e.button == MouseButton::Left)
      finish(e);
  };
  v.onPointerCancel = [finish](PointerEvent &e) {
    finish(e);
  }; // focus loss / the OS took the pointer: the release never comes
  if (hover != Cursor::Inherit) {
    const Cursor held = dragging == Cursor::Inherit ? hover : dragging;
    v.style.cursor = std::function<Cursor()>(
        [owner, hover, held] { return *owner ? held : hover; });
  }
}

// Every view with an onClick shows the hand cursor (Style::cursor is inherited,
// so its label does too), unless it already chose a cursor of its own. Run once
// on the finished tree.
static void applyClickCursors(View &v) {
  const auto *c = std::get_if<Cursor>(&v.style.cursor);
  if (v.onClick && c && *c == Cursor::Inherit)
    v.style.cursor = Cursor::Pointer;
  for (View &child : v.children)
    applyClickCursors(child);
}

// One row of a dropdown: label, action, and an optional shortcut hint shown
// right-aligned (display only — the real bindings are in main()).
struct MenuItem {
  std::string label;
  std::function<void()> action;
  std::string shortcut = {}; // optional
};

// A titlebar menu button ("File", "View", ...) that opens a dropdown of
// MenuItems below it when clicked. Only one menu is open at a time
// (EditorState::openMenu); picking an item runs its action and closes the
// menu.
static View menuButton(const std::string &label, int id,
                       std::vector<MenuItem> items) {
  View btn;
  btn.style.height = Size::full();
  btn.style.padding = EdgeInsets{0, 12, 0, 12};
  btn.style.justifyContent = Justify::Center;
  btn.style.alignItems = Align::Center;
  btn.style.hoverColor = kColorMenuHover;
  btn.style.backgroundColor = std::function<Color()>([id] {
    return state.openMenu == id ? kColorMenuHover : kColorTransparent;
  });
  btn.onClick = [id] { state.openMenu = (state.openMenu == id) ? -1 : id; };
  swallowPress(
      btn); // also covers the dropdown rows (they are btn's descendants)

  Text label_;
  label_.label = label;
  label_.fontSize = 13.0f;
  label_.color = kColorTextDark;
  btn.addChild(std::move(label_));

  View dropdown;
  dropdown.style.position = Position::Absolute;
  dropdown.style.top = kTitlebarH;
  dropdown.style.left = 0.0f;
  dropdown.style.width = Size::pixel(250);
  dropdown.style.direction = FlexDirection::Column;
  dropdown.style.backgroundColor = kColorDropdownBg;
  dropdown.style.borderWidth = 1.0f;
  dropdown.style.borderColor = kColorDropdownBorder;
  dropdown.style.borderRadius = 4.0f;
  dropdown.style.padding = EdgeInsets::all(4.0f);
  dropdown.style.zIndex = 100;
  dropdown.style.display = std::function<Display()>(
      [id] { return state.openMenu == id ? Display::Flex : Display::None; });

  for (auto &item : items) {
    View row;
    row.style.height = Size::pixel(30);
    row.style.width = Size::full();
    row.style.alignItems = Align::Center;
    row.style.justifyContent = Justify::SpaceBetween;
    row.style.padding = EdgeInsets{0, 10, 0, 10};
    row.style.borderRadius = 3.0f;
    row.style.hoverColor = kColorDropdownRowHover;
    std::function<void()> action = item.action;
    row.onClick = [action] {
      action();
      state.openMenu = -1;
    };

    Text t;
    t.label = item.label;
    t.fontSize = 13.0f;
    t.color = kColorTextDark;
    row.addChild(std::move(t));
    if (!item.shortcut.empty()) {
      Text hint;
      hint.label = item.shortcut;
      hint.fontSize = 12.0f;
      hint.color = kColorTextFaint;
      row.addChild(std::move(hint));
    }
    dropdown.addChild(std::move(row));
  }

  btn.addChild(std::move(dropdown));
  return btn;
}

// One of the three window-control buttons (minimize/maximize/close).
static View winButton(const std::string &glyph, Color hoverBg,
                      std::function<void()> onClick) {
  View v;
  v.style.width = Size::pixel(46);
  v.style.height = Size::full();
  v.style.justifyContent = Justify::Center;
  v.style.alignItems = Align::Center;
  v.style.hoverColor = hoverBg;
  v.onClick = std::move(onClick);
  swallowPress(v);

  Text t;
  t.label = glyph;
  t.fontSize = 13.0f;
  t.color = kColorTextMid;
  v.addChild(std::move(t));
  return v;
}

// A small square titlebar icon button (undo/redo, etc). `enabled` is
// optional: pass it to dim the glyph when the action isn't currently
// available — the click itself is always safe to wire up as a no-op in
// that case (undo()/redo() already check before doing anything).
static View titlebarIconBtn(const std::string &glyph,
                            std::function<void()> onClick,
                            std::function<bool()> enabled = {}) {
  View v;
  v.style.width = Size::pixel(28);
  v.style.height = Size::pixel(28);
  v.style.borderRadius = 4.0f;
  v.style.justifyContent = Justify::Center;
  v.style.alignItems = Align::Center;
  v.style.hoverColor = kColorMenuHover;
  v.onClick = std::move(onClick);
  swallowPress(v);

  Text t;
  t.label = glyph;
  t.fontSize = 14.0f;
  t.color = enabled ? Dynamic<Color>(std::function<Color()>([enabled] {
    return enabled() ? kColorTextDark : kColorTextFaint;
  }))
                    : Dynamic<Color>(kColorTextDark);
  v.addChild(std::move(t));
  return v;
}

// A full-width sidebar button. `selected` (optional) draws it highlighted —
// used for the aspect-ratio presets, which behave like radio buttons.
// `primary` gives it the accent fill for the pane's main action.
static View sidebarButton(const std::string &label,
                          std::function<void()> onClick,
                          std::function<bool()> selected = {},
                          bool primary = false,
                          std::function<bool()> enabled = {}) {
  View b;
  b.style.width = Size::full();
  b.style.height = Size::pixel(28);
  b.style.flexShrink = 0;
  b.style.borderRadius = 4.0f;
  b.style.alignItems = Align::Center;
  b.style.padding = EdgeInsets{0, 10, 0, 10};
  b.style.hoverColor = primary ? kColorAccentHover : kColorActivityHover;
  b.style.backgroundColor =
      std::function<Color()>([selected, primary, enabled]() -> Color {
        if (enabled && !enabled())
          return kColorActivityBarBg; // dimmed: no primary fill while busy
        if (primary)
          return kColorAccent;
        return (selected && selected()) ? kColorDropdownRowHover
                                        : kColorActivityBarBg;
      });
  b.style.borderWidth = std::function<float()>(
      [selected] { return (selected && selected()) ? 1.0f : 0.0f; });
  b.style.borderColor = kColorDividerHover;
  // A disabled button ignores clicks (the action functions also re-check, so
  // shortcuts are covered) and shows muted text.
  b.onClick = [onClick = std::move(onClick), enabled] {
    if (!enabled || enabled())
      onClick();
  };

  Text t;
  t.label = label;
  t.fontSize = 13.0f;
  const Color liveColor = primary ? kColorWhite : kColorTextDark;
  if (enabled)
    t.color = std::function<Color()>([enabled, liveColor]() -> Color {
      return enabled() ? liveColor : kColorTextMuted;
    });
  else
    t.color = liveColor;
  b.addChild(std::move(t));
  return b;
}

// What a slider row needs to know: its label, range, how to read/write the
// value, and how to print it. Shared by the Adjust sliders (via
// buildAdjustmentSlider) and the Straighten angle slider.
// Export's "Resize" slider: the long side, in px, in steps of
// kExportLongEdgeStep.
constexpr int kExportLongEdgeMin = 128;
constexpr int kExportLongEdgeMax = 8192;
constexpr int kExportLongEdgeStep = 16;

struct SliderSpec {
  std::string name;
  float lo = 0.0f, hi = 1.0f;
  std::function<float()> get;
  std::function<void(float)> set; // called with each new value while dragging
  std::function<std::string(float)> format; // value -> text after "name: "
  bool isAngle = false;      // drag drives the angle preview instead of the
                             // pixel-adjustment preview (see canvasDirtySource)
  bool affectsPixels = true; // false: the value only changes a tool setting
                             // (brush size...), so a drag needs no re-render
  bool hasAccent = false;    // tint the fill + thumb border (Color Mixer bands)
  Color accent{};
  float trackW = 150.0f;       // track length in px
  std::function<void()> onEnd; // optional: fired when a drag on it ends
};

// A single Lightroom-style slider row: a label with the live value, then a
// draggable track. Reads and writes go through the spec's get/set rather
// than binding a raw float& the way a standalone slider widget would —
// that's what lets AdjustmentSet coalesce a whole drag into one undo step
// instead of one per frame.
static View buildSliderRow(const SliderSpec &spec) {
  const float kTrackW = spec.trackW;
  constexpr float kTrackH = 4.0f;
  constexpr float kThumb = 16.0f;
  const float kMaxThumbX = kTrackW - kThumb;

  const SliderSpec s = spec;
  const float lo = s.lo;
  const float hi = s.hi;

  // Same bipolar-fill treatment as LiteUI's slider example: a range that
  // straddles zero (like ours, -100..100) fills outward from the center
  // instead of always from the left edge.
  const bool isBipolar = (lo < 0.0f && hi > 0.0f);
  const float anchor = isBipolar ? 0.0f : lo;

  auto thumbXFor = [lo, hi, kMaxThumbX](float v) {
    return std::clamp((v - lo) / (hi - lo), 0.0f, 1.0f) * kMaxThumbX;
  };

  View group;
  group.style.direction = FlexDirection::Column;
  group.style.gap = 6.0f;

  Text rowLabel;
  rowLabel.label = std::function<std::string()>(
      [s] { return s.name + ": " + s.format(s.get()); });
  rowLabel.fontSize = 13.0f;
  rowLabel.color = kColorTextMid;
  group.addChild(std::move(rowLabel));

  View track;
  track.style.width = Size::pixel(kTrackW);
  track.style.height = Size::pixel(kTrackH);
  track.style.backgroundColor = kTrackFill;
  track.style.borderWidth = 1.0f;
  track.style.borderColor = kTrackEdge;
  track.style.borderRadius = kTrackH / 2.0f;

  // Filled portion, absolutely positioned so it can grow independently of
  // the thumb. Spans from the anchor's thumb-center position to the
  // current value's.
  View fill;
  fill.style.position = Position::Absolute;
  fill.style.top = 0.0f;
  fill.style.height = Size::pixel(kTrackH);
  fill.style.left = std::function<float()>([s, thumbXFor, anchor, isBipolar] {
    float anchorX =
        isBipolar ? thumbXFor(anchor) + kThumb / 2.0f : thumbXFor(anchor);
    float valueCenter = thumbXFor(s.get()) + kThumb / 2.0f;
    return std::min(anchorX, valueCenter);
  });
  fill.style.width = std::function<Size()>([s, thumbXFor, anchor, isBipolar] {
    float anchorX =
        isBipolar ? thumbXFor(anchor) + kThumb / 2.0f : thumbXFor(anchor);
    float valueCenter = thumbXFor(s.get()) + kThumb / 2.0f;
    return Size::pixel(std::abs(valueCenter - anchorX));
  });
  fill.style.backgroundColor = s.hasAccent ? s.accent : kTrackEdge;
  fill.style.borderRadius = kTrackH / 2.0f;
  track.addChild(std::move(fill));

  // Shared between the press and the drag moves of one gesture, same
  // pattern as the standalone slider widget this is adapted from.
  auto grabX = std::make_shared<float>(0.0f);

  View thumb;
  thumb.style.position = Position::Absolute;
  thumb.style.left =
      std::function<float()>([s, thumbXFor] { return thumbXFor(s.get()); });
  thumb.style.top = -(kThumb - kTrackH) / 2.0f; // vertically centered
  thumb.style.width = Size::pixel(kThumb);
  thumb.style.height = Size::pixel(kThumb);
  thumb.style.backgroundColor = kThumbFill;
  thumb.style.borderWidth = 2.0f;
  thumb.style.borderColor = s.hasAccent ? s.accent : kThumbEdge;
  thumb.style.borderRadius = kThumb / 2.0f;

  auto thumbPress = [grabX](float lx, float) { *grabX = lx; };
  auto thumbDrag = [grabX, s, thumbXFor, lo, hi, kMaxThumbX](float lx, float) {
    if (!state.original)
      return; // no image loaded yet — nothing to adjust
    float newThumbX =
        std::clamp(thumbXFor(s.get()) + (lx - *grabX), 0.0f, kMaxThumbX);
    s.set(lo + (newThumbX / kMaxThumbX) * (hi - lo));
    if (!s.affectsPixels) {
      state.markDirty(); // just repaint (an overlay may show the setting)
      return;
    }
    enterDrag(s.isAngle); // switches recompute+paint to the preview path
    // Cheap: just records the new value and asks for a recompute before
    // the next frame paints (see canvasDirtySource) — doesn't itself touch
    // a single pixel, so it stays fast no matter how often the drag fires.
    state.markAdjustDirty();
  };
  // Drag finished: leave preview mode and request one full-resolution
  // recompute (deferred to the next canvasDirtySource poll like any other
  // adjustment change — endPress polls right after this fires, so it lands
  // in the same frame the mouse is released).
  auto thumbEnd = [s] {
    if (s.onEnd)
      s.onEnd();
    if (!state.dragging)
      return;
    state.dragging = false;
    state.dragIsAngle = false;
    state.markAdjustDirty();
  };
  bindDrag(thumb, thumbPress, thumbDrag, thumbEnd, nullptr, Cursor::Grab,
           Cursor::Grabbing);
  track.addChild(std::move(thumb));

  group.addChild(std::move(track));
  return group;
}

// One Adjust-pane slider for one AdjustmentSpec: adding a new adjustment to
// kAdjustmentSpecs needs no new UI code at all.
static View buildAdjustmentSlider(const AdjustmentSpec &spec) {
  const AdjId id = spec.id;
  SliderSpec s;
  s.name = spec.label;
  s.lo = spec.min;
  s.hi = spec.max;
  s.get = [id] { return state.adjust.get(id); };
  s.set = [id](float v) { state.adjust.set(id, v); };
  s.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v)));
  };
  return buildSliderRow(s);
}

