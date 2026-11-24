// app/panes/export_pane.hpp: the Export pane (buildExportPane)
//
// UI-shell code split out of main(). The function body is the code that used
// to sit inline in main(), unchanged. Included only by main.cpp, after the
// other app/ files and before main(); not self-contained.
#pragma once

// -- "Export" pane: format, JPEG quality, metadata, and the Export button.
// (The File menu's Export... and Ctrl+E use these same settings.)
static View buildExportPane() {
  View exportPane;
  exportPane.style.width = Size::full();
  exportPane.style.direction = FlexDirection::Column;
  exportPane.style.gap = 6.0f;
  exportPane.style.display = std::function<Display()>(
      [] { return state.activeActivity == kExportActivity ? Display::Flex : Display::None; });

  Text exportHint;
  exportHint.label = std::function<std::string()>([]() -> std::string {
    return state.image ? "Saves the picture as you see it, with every edit, "
                         "crop and rotation applied."
                       : "Open an image to export it.";
  });
  exportHint.fontSize = 12.0f;
  exportHint.color = kColorTextMuted;
  exportHint.style.margin = EdgeInsets{0, 0, 6, 0};
  exportPane.addChild(std::move(exportHint));

  exportPane.addChild(sidebarButton(
      "PNG  (lossless)",
      [] {
        state.exportJpeg = false;
        state.exportPng16 = false;
      },
      [] { return !state.exportJpeg && !state.exportPng16; }));
  exportPane.addChild(sidebarButton(
      "PNG  16-bit",
      [] {
        state.exportJpeg = false;
        state.exportPng16 = true;
      },
      [] { return !state.exportJpeg && state.exportPng16; }));
  exportPane.addChild(sidebarButton(
      "JPEG",
      [] {
        state.exportJpeg = true;
        state.exportPng16 = false;
      },
      [] { return state.exportJpeg; }));

  Text png16Hint;
  png16Hint.label = std::function<std::string()>([]() -> std::string {
    return "16 bits per channel: smooth gradients survive heavy edits. "
           "Larger files, and not every program opens them.";
  });
  png16Hint.fontSize = 11.0f;
  png16Hint.color = kColorTextMuted;
  png16Hint.style.margin = EdgeInsets{4, 0, 0, 0};
  png16Hint.style.display = std::function<Display()>([] {
    return !state.exportJpeg && state.exportPng16 ? Display::Flex
                                                  : Display::None;
  });
  exportPane.addChild(std::move(png16Hint));

  View jpegOptions;
  jpegOptions.style.width = Size::full();
  jpegOptions.style.direction = FlexDirection::Column;
  jpegOptions.style.gap = 6.0f;
  jpegOptions.style.margin = EdgeInsets{6, 0, 0, 0};
  jpegOptions.style.display = std::function<Display()>(
      [] { return state.exportJpeg ? Display::Flex : Display::None; });

  SliderSpec qualitySlider;
  qualitySlider.name = "Quality";
  qualitySlider.lo = 1.0f;
  qualitySlider.hi = 100.0f;
  qualitySlider.get = [] { return static_cast<float>(state.jpegQuality); };
  qualitySlider.set = [](float v) {
    state.jpegQuality = std::clamp(static_cast<int>(std::lround(v)), 1, 100);
  };
  qualitySlider.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v)));
  };
  qualitySlider.affectsPixels = false; // a setting, not an edit: no re-render
  jpegOptions.addChild(buildSliderRow(qualitySlider));

  jpegOptions.addChild(sidebarButton(
      "Keep EXIF & color profile",
      [] { state.keepMetadata = !state.keepMetadata; },
      [] { return state.keepMetadata; }));
  // Only meaningful with the metadata kept (without it there is no GPS block to
  // carry), so it is dimmed otherwise.
  jpegOptions.addChild(sidebarButton(
      "Remove location (GPS)", [] { state.removeGps = !state.removeGps; },
      [] { return state.removeGps && state.keepMetadata; }, false,
      [] { return state.keepMetadata; }));
  exportPane.addChild(std::move(jpegOptions));

  // Resize applies to both formats, so it sits outside the JPEG-only options.
  View resizeOptions;
  resizeOptions.style.width = Size::full();
  resizeOptions.style.direction = FlexDirection::Column;
  resizeOptions.style.gap = 6.0f;
  resizeOptions.style.margin = EdgeInsets{6, 0, 0, 0};
  resizeOptions.addChild(sidebarButton(
      "Resize to long edge", [] { state.exportResize = !state.exportResize; },
      [] { return state.exportResize; }));

  View resizeDetail;
  resizeDetail.style.width = Size::full();
  resizeDetail.style.direction = FlexDirection::Column;
  resizeDetail.style.gap = 6.0f;
  resizeDetail.style.display = std::function<Display()>(
      [] { return state.exportResize ? Display::Flex : Display::None; });

  SliderSpec edgeSlider;
  edgeSlider.name = "Long edge";
  edgeSlider.lo = static_cast<float>(kExportLongEdgeMin);
  edgeSlider.hi = static_cast<float>(kExportLongEdgeMax);
  edgeSlider.get = [] { return static_cast<float>(state.exportLongEdge); };
  edgeSlider.set = [](float v) {
    const int stepped = static_cast<int>(std::lround(v / kExportLongEdgeStep)) *
                        kExportLongEdgeStep;
    state.exportLongEdge =
        std::clamp(stepped, kExportLongEdgeMin, kExportLongEdgeMax);
  };
  edgeSlider.format = [](float v) {
    return std::to_string(static_cast<int>(std::lround(v))) + " px";
  };
  edgeSlider.affectsPixels = false; // a setting, not an edit: no re-render
  resizeDetail.addChild(buildSliderRow(edgeSlider));

  Text resizeInfo;
  resizeInfo.label = std::function<std::string()>([]() -> std::string {
    if (!state.image)
      return "";
    const int w = state.image->width, h = state.image->height;
    const Dims d = fitLongEdge(w, h, state.exportLongEdge);
    if (d.w == w && d.h == h)
      return "Already within " + std::to_string(state.exportLongEdge) +
             " px: exported at full size (" + std::to_string(w) + " × " +
             std::to_string(h) + ").";
    return "Exports at " + std::to_string(d.w) + " × " + std::to_string(d.h) +
           " px (from " + std::to_string(w) + " × " + std::to_string(h) + ").";
  });
  resizeInfo.fontSize = 12.0f;
  resizeInfo.color = kColorTextMuted;
  resizeDetail.addChild(std::move(resizeInfo));
  resizeOptions.addChild(std::move(resizeDetail));
  exportPane.addChild(std::move(resizeOptions));

  View exportGap;
  exportGap.style.height = Size::pixel(6);
  exportPane.addChild(std::move(exportGap));
  exportPane.addChild(sidebarButton("Export...", exportImage, {}, true,
                                    [] { return state.image != nullptr; }));

  // Batch export: the same settings, applied to the filmstrip's pictures.
  View batchOptions;
  batchOptions.style.width = Size::full();
  batchOptions.style.direction = FlexDirection::Column;
  batchOptions.style.gap = 6.0f;
  batchOptions.style.margin = EdgeInsets{12, 0, 0, 0};
  Text batchHint;
  batchHint.label = std::function<std::string()>([]() -> std::string {
    if (workers().exporter.busy())
      return "Exporting " + std::to_string(workers().exporter.done()) + " of " +
             std::to_string(workers().exporter.total()) + "...";
    return "Batch: exports the picked filmstrip photos (Ctrl+click, "
           "Shift+click) to one folder, each with its own edits.";
  });
  batchHint.fontSize = 12.0f;
  batchHint.color = kColorTextMuted;
  batchOptions.addChild(std::move(batchHint));
  batchOptions.addChild(sidebarButton(
      "Overwrite existing files",
      [] { state.exportOverwrite = !state.exportOverwrite; },
      [] { return state.exportOverwrite; }));
  batchOptions.addChild(
      sidebarButton("Export Selected...", exportSelected, {}, true, [] {
        return !state.film.files.empty() && !workers().exporter.busy();
      }));
  batchOptions.addChild(
      sidebarButton("Export All Shown...", exportAllShown, {}, false, [] {
        return !state.film.files.empty() && !workers().exporter.busy();
      }));
  batchOptions.addChild(sidebarButton("Stop Export", stopExport, {}, false,
                                      [] { return workers().exporter.busy(); }));
  exportPane.addChild(std::move(batchOptions));

  return exportPane;
}
