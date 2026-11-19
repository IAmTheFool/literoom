// app/saved_edits.hpp: saved edits: the sidecar file
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// ---------------- Saved edits: the sidecar file ----------------
// photo.jpg's edits live in photo.jpg.literoom.json beside it. They are
// written (temp file, then rename, so a crash can't leave half a file)
//   - after any change, once no mouse drag is in progress (autosaveSidecar,
//     polled every frame from flushPendingAdjust),
//   - when another picture replaces this one, and when the app closes;
// and read back when the picture is opened. A missing file is normal; a
// damaged one, one from a newer version, or one written for a different-sized
// picture is ignored (the editor starts clean) and the status bar says so.
// Opening a picture and touching nothing writes nothing. The undo history is
// not saved.

static std::filesystem::path sidecarPath(const std::string &imagePath) {
  return fsPath(sidecarFileOf(imagePath));
}

// A picture's name for messages and the title bar: a virtual copy shows as
// "photo.jpg [Copy 2]".
static std::string pictureLabel(const std::string &path) {
  std::string s = baseName(vcImageOf(path));
  if (const int n = vcCopyOf(path); n > 0)
    s += " [Copy " + std::to_string(n) + "]";
  return s;
}

// The open picture's camera data for the Info pane (make, model, lens, ISO,
// shutter, ...), read from the file once per opened picture: the first time the
// pane asks after imageSerial changed. It reads the first 256 KiB of the file
// on the UI thread, which is a fraction of a millisecond; nothing here touches
// the render pipeline. A picture with no readable camera data (a PNG, a JPEG
// without EXIF) gives an empty PhotoInfo.
static const exif::PhotoInfo &currentPhotoInfo() {
  if (state.photoInfoSerial != state.imageSerial) {
    state.photoInfoSerial = state.imageSerial;
    state.photoInfo = exif::PhotoInfo{};
    if (!state.imagePath.empty())
      photoInfoOf(state.imagePath, &state.photoInfo);
  }
  return state.photoInfo;
}

// What the open file says its color space is, for the Info pane and the status
// bar: read once per opened picture, like currentPhotoInfo (a PNG's color
// chunks, or a JPEG's ICC profile). The editor converts nothing: it edits the
// file's numbers as they are and carries the tag into PNG exports.
static const color_tag::ColorTag &currentColorTag() {
  if (state.colorTagSerial != state.imageSerial) {
    state.colorTagSerial = state.imageSerial;
    state.colorTag = color_tag::ColorTag{};
    if (!state.imagePath.empty())
      state.colorTag = color_tag::colorTagOf(vcImageOf(state.imagePath));
  }
  return state.colorTag;
}

// The status-bar notice for the file just opened: what it says its color space
// is, for every file (sRGB, untagged, or something else). A picture tagged as
// something other than sRGB also gets the reminder that nothing is converted.
// Short on purpose: setStatus cuts a message at 110 characters, and "Color:
// <profile> (embedded profile, not sRGB)" is already about 50 to 90 of them, so
// the part that matters comes first and the rest is brief.
static void announceColorTag() {
  const color_tag::ColorTag &tag = currentColorTag();
  if (tag.text.empty())
    return; // the file could not be read
  setStatus("Color: " + tag.text +
                (tag.srgb ? std::string()
                          : std::string(". Shown as sRGB, not converted")),
            false);
}

// Writes the sidecar for the open picture. On failure (read-only folder...)
// the status bar says so; the change still counts as "seen" so a failing
// disk isn't retried every frame, only on the next edit.
static void saveSidecar() {
  if (!state.original || state.imagePath.empty())
    return;
  const bool editsChanged = state.adjust.version() != state.sidecarSaved;
  state.sidecarSaved = state.adjust.version();
  state.metaDirty = false;
  state.snapshotsDirty = false;

  const std::string text =
      sidecarFileText(baseName(vcImageOf(state.imagePath)),
                      state.original->width, state.original->height,
                      state.adjust.toJson(), state.meta, state.snapshots);

  const std::filesystem::path path = sidecarPath(state.imagePath);
  std::filesystem::path tmp = path;
  tmp += ".tmp";
  std::error_code ec;
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    f.flush();
    if (!f) {
      f.close();
      std::filesystem::remove(tmp, ec);
      setStatus("Couldn't save edits next to " + pictureLabel(state.imagePath),
                true);
      return;
    }
  }
  std::filesystem::rename(tmp, path, ec); // replaces an existing file
  if (ec) {
    std::error_code ignore;
    std::filesystem::remove(tmp, ignore);
    setStatus("Couldn't save edits: " + ec.message(), true);
  } else if (editsChanged)
    filmMarkEdited(state.imagePath); // the strip's "has edits" dot (a rating
                                     // alone doesn't set it)
}

// Saves now if anything changed since the last save / load.
static void flushSidecar() {
  if (state.original && (state.adjust.version() != state.sidecarSaved ||
                         state.metaDirty || state.snapshotsDirty))
    saveSidecar();
}

// Per frame: saves a finished change, but not while the mouse is still
// dragging a slider or a mask (that would write on every frame).
static void autosaveSidecar() {
  if (!state.original || (state.adjust.version() == state.sidecarSaved &&
                          !state.metaDirty && !state.snapshotsDirty))
    return;
  if (state.dragging || state.mask.drag != MaskUi::Drag::None)
    return;
  saveSidecar();
}

