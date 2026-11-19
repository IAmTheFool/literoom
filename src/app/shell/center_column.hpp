// app/shell/center_column.hpp: canvas, "no image" placeholder and filmstrip (buildCenterColumn)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// ---- Center column: canvas viewport on top, filmstrip underneath ----
// Keeps the filmstrip under the picture only, so the sidebar keeps its
// full height.
static View buildCenterColumn() {
  View centerColumn;
  centerColumn.style.direction = FlexDirection::Column;
  centerColumn.style.height = Size::full();
  centerColumn.style.flexGrow = 1;

  // ---- Canvas: fills the whole area under the toolbar ----
  // The canvas is as big as that area, whatever the zoom; paintCanvas draws the
  // picture into it, centered or scrolled (see ViewGeo). Only occupies the slot
  // while an image is open; the "no image" placeholder below is the other child
  // for that same slot.
  View canvas;
  canvas.isCanvas = true;
  canvas.style.width = Size::full();
  canvas.style.flexGrow = 1;
  canvas.style.display = std::function<Display()>(
      [] { return state.image ? Display::Flex : Display::None; });
  // The final size, on every relayout (window resize included).
  canvas.onLayout = [](float, float, float w, float h) {
    state.view.w = w;
    state.view.h = h;
    viewGeo(); // keep the scroll offsets in range
  };

  canvas.onPaint = [](CanvasContext &ctx) { paintCanvas(ctx); };
  canvas.canvasDirtySource = [] {
    // Polled once per frame (see liteui.hpp's checkForUpdates) — the right
    // place to do the deferred, expensive part of a slider drag exactly
    // once per frame, no matter how many pointer-move events fired since the
    // last poll.
    flushPendingAdjust();
    bool d = state.dirty;
    state.dirty = false;
    return d;
  };
  // RGB readout under the cursor. Pointer moves have no return value; a
  // change just marks the canvas dirty so the next poll repaints it.
  canvas.onPointerLeave = [](PointerEvent &) {
    if (hoverClear())
      state.dirty = true;
  };
  // Crop-selection dragging (no-ops unless the Crop pane is open).
  // Each is a no-op unless its own pane is open (crop: 2, masks: 7).
  // While comparing, the canvas does nothing but move the split line.
  // `pressed` marks a left press that started on this canvas, so moves and
  // the release of some other gesture crossing it are not taken for a drag.
  // (one pointer owns the gesture, as in bindDrag: a second finger is ignored)
  auto pressed = std::make_shared<std::optional<uint32_t>>();
  // The pointer that owns a view drag (scrollbar thumb, or panning with the
  // middle button / the left one while no tool is active).
  auto viewPointer = std::make_shared<std::optional<uint32_t>>();
  canvas.onPointerDown = [pressed, viewPointer](PointerEvent &e) {
    e.stopPropagation();
    if (e.button == MouseButton::Left)
      closeMenus(); // this press never reaches the workspace's handler
    if (*viewPointer)
      return; // another pointer is already moving the view
    if (e.button == MouseButton::Left && viewBarPress(e.x, e.y)) {
      if (state.view.drag != ViewDrag::None)
        *viewPointer = e.pointerId;
      return;
    }
    if (e.button == MouseButton::Middle) {
      viewPanBegin(e.x, e.y);
      *viewPointer = e.pointerId;
      return;
    }
    // The tools work in picture-local coordinates (divide by the zoom for
    // image pixels): the canvas coordinates minus the picture's corner.
    float lx, ly;
    viewLocal(e.x, e.y, lx, ly);
    if (e.button == MouseButton::Right) {
      if (state.compare == CompareMode::Off) {
        objectPress(lx, ly, true);
        polygonRightPress(lx, ly);
      }
      return;
    }
    if (e.button != MouseButton::Left)
      return;
    if (*pressed)
      return; // another pointer is already dragging on the canvas
    *pressed = e.pointerId;
    if (state.compare != CompareMode::Off) {
      if (state.compare == CompareMode::Split)
        splitFromX(lx);
      return;
    }
    if (!wbPickerActive() && !cropToolActive() && !maskToolActive()) {
      // No tool wants the drag: it pans the view.
      pressed->reset();
      viewPanBegin(e.x, e.y);
      *viewPointer = e.pointerId;
      return;
    }
    if (wbPress(lx, ly)) {
      state.openMenu = -1; // same as cropPress: a canvas press closes any menu
      return;
    }
    cropPress(lx, ly);
    maskPress(lx, ly);
  };
  canvas.onPointerMove = [pressed, viewPointer](PointerEvent &e) {
    if (*viewPointer == e.pointerId) {
      viewDragTo(e.x, e.y);
      return;
    }
    float lx, ly;
    viewLocal(e.x, e.y, lx, ly);
    if (hoverSample(lx, ly))
      state.dirty = true;
    if (*pressed != e.pointerId || !e.held(MouseButton::Left))
      return;
    if (state.compare != CompareMode::Off) {
      if (state.compare == CompareMode::Split)
        splitFromX(lx);
      return;
    }
    cropDrag(lx, ly);
    maskDrag(lx, ly);
  };
  auto endCanvasDrag = [pressed](const PointerEvent &e) {
    if (*pressed != e.pointerId)
      return;
    pressed->reset();
    state.crop.drag = CropUi::Drag::None;
    if (state.mask.drag != MaskUi::Drag::None)
      maskEndDrag();
  };
  auto endViewDrag = [viewPointer](const PointerEvent &e) {
    if (*viewPointer != e.pointerId)
      return;
    viewPointer->reset();
    state.view.drag = ViewDrag::None;
  };
  canvas.onPointerUp = [endCanvasDrag, endViewDrag](PointerEvent &e) {
    endViewDrag(e);
    if (e.button == MouseButton::Left)
      endCanvasDrag(e);
  };
  canvas.onPointerCancel = [endCanvasDrag, endViewDrag](PointerEvent &e) {
    endViewDrag(e);
    endCanvasDrag(e);
  };
  // Cursor: the split line drags sideways; the tools and the picker aim.
  canvas.style.cursor = std::function<Cursor()>([]() -> Cursor {
    if (state.compare == CompareMode::Split)
      return Cursor::ResizeEW;
    if (state.compare != CompareMode::Off)
      return Cursor::Default;
    if (wbPickerActive() || cropToolActive() || maskToolActive())
      return Cursor::Crosshair;
    return Cursor::Default;
  });
  // Wheel zoom. A notched wheel steps by kZoomStep; a trackpad (precise
  // pixel deltas) zooms continuously. +dy = wheel down = zoom out.
  canvas.onWheel = [](WheelEvent &e) {
    if (e.dy == 0.0f)
      return;
    const float f = e.precise ? std::pow(kZoomStep, -e.dy / 40.0f)
                              : (e.dy < 0.0f ? kZoomStep : 1.0f / kZoomStep);
    zoomAbout(state.zoomDev * f, e.x, e.y); // the point under the pointer stays
    e.stopPropagation();
  };

  centerColumn.addChild(std::move(canvas));

  View noImagePlaceholder;
  noImagePlaceholder.style.width = Size::full();
  noImagePlaceholder.style.flexGrow = 1;
  noImagePlaceholder.style.backgroundColor = kColorViewportBg;
  noImagePlaceholder.style.justifyContent = Justify::Center;
  noImagePlaceholder.style.alignItems = Align::Center;
  noImagePlaceholder.style.display = std::function<Display()>(
      [] { return state.image ? Display::None : Display::Flex; });

  Text noImageText;
  noImageText.label = std::string("No image selected");
  noImageText.fontSize = 14.0f;
  noImageText.color = kColorTextFaint;
  noImagePlaceholder.addChild(std::move(noImageText));

  centerColumn.addChild(std::move(noImagePlaceholder));

  // Filmstrip: the open folder's pictures, under the canvas.
  View filmstrip;
  filmstrip.isCanvas = true;
  filmstrip.style.width = Size::full();
  filmstrip.style.flexShrink = 0;
  filmstrip.style.height = std::function<Size()>(
      [] { return Size::pixel(filmstripShown() ? kFilmH : 0.0f); });
  filmstrip.style.display = std::function<Display()>(
      [] { return filmstripShown() ? Display::Flex : Display::None; });
  filmstrip.onPaint = [](CanvasContext &ctx) { paintFilmstrip(ctx); };
  filmstrip.style.cursor = Cursor::Pointer;
  filmstrip.canvasDirtySource = [] {
    collectThumbJobs();
    const bool d = state.film.dirty;
    state.film.dirty = false;
    return d;
  };
  filmstrip.onPointerDown = [](PointerEvent &e) {
    if (e.button != MouseButton::Left)
      return;
    e.stopPropagation();
    closeMenus();
    filmClick(e.x, e.y, e.mods); // modifiers now come with the event
  };
  // Wheel: two cells per notch; a trackpad scrolls by its pixel delta (and
  // honours sideways swipes).
  filmstrip.onWheel = [](WheelEvent &e) {
    if (e.precise)
      filmScrollBy(std::abs(e.dx) > std::abs(e.dy) ? e.dx : e.dy);
    else if (e.dy != 0.0f)
      filmScrollBy((e.dy < 0.0f ? -2.0f : 2.0f) * kFilmCellW);
    else
      return;
    e.stopPropagation();
  };
  centerColumn.addChild(std::move(filmstrip));

  return centerColumn;
}
