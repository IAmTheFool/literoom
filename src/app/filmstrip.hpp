// app/filmstrip.hpp: the filmstrip (fence: filmstrip)
//
// UI-shell code split out of main.cpp. A plain move: nothing was changed.
// Included only by main.cpp, at the spot where this code used to be, so it
// is not self-contained: it uses what main.cpp defines before it.
#pragma once

// BEGIN filmstrip
// ---------------- Filmstrip: the folder's pictures ----------------
// A strip under the canvas with a thumbnail of every JPEG / PNG in the open
// picture's folder. Click one (or press Left / Right) to open it; a dot marks
// pictures that have saved edits. It is one canvas that paints only the cells
// in view, so a folder of thousands costs nothing extra. Thumbnails are made
// by a background thread, nearest-to-the-open-picture first, and the open
// picture's thumbnail is refreshed (edits included) when you leave it.
inline constexpr float kFilmBarH = 26.0f;    // the filter bar on top of the cells
inline constexpr float kFilmCellsH = 104.0f; // the row of thumbnails below it
inline constexpr float kFilmH = kFilmBarH + kFilmCellsH;
inline constexpr float kFilmCellW = 108.0f;
inline constexpr size_t kFilmMaxFiles = 5000; // a bigger folder shows its first 5000

// makeThumb / ThumbWorker / kFilmThumbW,H live in thumb_worker.hpp.
// (the one ThumbWorker is workers().thumb, app/workers.hpp)

// Lists the JPEG / PNG files of `folder` into a new strip (not yet shown).
using VcMap = std::map<std::string, std::vector<int>>; // see listVirtualCopies

// Adds one entry. The name index is keyed by the path's own last part, so a
// virtual copy ("photo.jpg|vc2") never collides with its picture.
inline void filmPushEntry(FilmStrip &f, const std::string &path,
                          const std::string &name, int copy) {
  FilmEntry e;
  e.path = path;
  e.name = name;
  e.copy = copy;
  f.byName[filmLower(baseName(path))] = f.files.size();
  f.files.push_back(std::move(e));
}

// A picture, then its virtual copies (`copies`: the folder's, by lower-case
// name).
inline void filmPushWithCopies(FilmStrip &f, const std::string &path,
                               const std::string &name, const VcMap &copies) {
  filmPushEntry(f, path, name, 0);
  if (const auto it = copies.find(filmLower(name)); it != copies.end())
    for (const int n : it->second)
      filmPushEntry(f, vcPath(path, n), name, n);
}

inline FilmStrip filmListFolder(const std::string &folder) {
  FilmStrip f;
  f.folder = folder;
  const std::filesystem::path base = fsPath(folder);
  const VcMap copies = listVirtualCopies(folder, kFilmMaxFiles);
  for (auto &name : listImageFiles(folder, kFilmMaxFiles))
    filmPushWithCopies(f, u8ToString(base / fsPath(name)), name, copies);
  return f;
}

// The strip shows `view`: the files that pass the filter. The open picture and
// the one being opened always stay in, so rating the picture you are looking at
// under a filter can't make it vanish. UI thread only.
inline void filmRebuildView() {
  FilmStrip &f = state.film;
  f.view.clear();
  const bool all = !f.filter.active();
  for (size_t i = 0; i < f.files.size(); ++i) {
    const int ii = static_cast<int>(i);
    if (all || ii == f.current || ii == f.target ||
        f.filter.matches(f.files[i].meta))
      f.view.push_back(ii);
  }
  f.dirty = true;
}

// Where file `i` sits in the strip, or -1 if it isn't shown.
inline int filmViewPos(int i) {
  const std::vector<int> &v = state.film.view;
  const auto it = std::lower_bound(v.begin(), v.end(), i);
  return it != v.end() && *it == i ? static_cast<int>(it - v.begin()) : -1;
}

inline void filmClampScroll() {
  FilmStrip &f = state.film;
  const float total = static_cast<float>(f.view.size()) * kFilmCellW;
  f.scrollX = std::clamp(f.scrollX, 0.0f, std::max(0.0f, total - f.viewW));
}

inline void filmEnsureVisible(int i) {
  FilmStrip &f = state.film;
  const int pos = i < 0 ? -1 : filmViewPos(i);
  if (pos < 0)
    return;
  const float x0 = static_cast<float>(pos) * kFilmCellW, x1 = x0 + kFilmCellW;
  const float w = f.viewW > 0.0f ? f.viewW : 800.0f; // before the first paint
  if (x0 < f.scrollX)
    f.scrollX = x0;
  else if (x1 > f.scrollX + w)
    f.scrollX = x1 - w;
  filmClampScroll();
  f.dirty = true;
}

// Points the thumbnail worker at the pictures that still lack one, nearest
// to the target first.
inline void filmQueueThumbs() {
  const FilmStrip &f = state.film;
  const int n = static_cast<int>(f.files.size());
  std::vector<std::string> todo;
  auto want = [&](int i) {
    const FilmEntry &e = f.files[static_cast<size_t>(i)];
    if (!e.thumb && !e.failed)
      todo.push_back(e.path);
  };
  const int mid =
      std::clamp(f.target >= 0 ? f.target : f.current, 0, std::max(0, n - 1));
  if (n > 0) {
    want(mid);
    for (int d = 1; mid - d >= 0 || mid + d < n; ++d) {
      if (mid + d < n)
        want(mid + d);
      if (mid - d >= 0)
        want(mid - d);
    }
  }
  workers().thumb.setQueue(std::move(todo));
}

// A strip holding exactly `paths` (already sorted). All come from one folder.
inline void filmAdoptFiles(const std::vector<std::string> &paths) {
  FilmStrip next;
  next.visible = state.film.visible;
  VcMap copies;
  for (const std::string &p : paths) {
    // A virtual copy's path adopts its picture and all of that picture's
    // copies.
    const std::string real = vcImageOf(p);
    if (next.folder.empty()) {
      std::string dir = u8ToString(fsPath(real).parent_path());
      next.folder = dir.empty() ? "." : dir;
      copies = listVirtualCopies(next.folder, kFilmMaxFiles);
    }
    if (next.byName.count(filmLower(baseName(real))))
      continue; // already in
    filmPushWithCopies(next, real, baseName(real), copies);
  }
  state.film = std::move(next);
  filmRebuildView(); // a new strip starts with no filter
  state.film.dirty = true;
}

// Makes the folder `folder` the filmstrip's. False (nothing changed) if it
// holds no JPEG / PNG pictures.
inline bool filmAdoptFolder(const std::string &folder) {
  FilmStrip next = filmListFolder(folder);
  if (next.files.empty())
    return false;
  next.visible = state.film.visible;
  state.film = std::move(next);
  filmRebuildView(); // a new strip starts with no filter
  state.film.dirty = true;
  return true;
}

// The open picture changed (or was just opened): follow it. Re-lists the
// folder if it is a different one, selects the picture, scrolls to it and
// re-aims the thumbnail worker. Call after loadSidecar().
inline void filmSync(const std::string &imagePath) {
  FilmStrip &f = state.film;
  const std::filesystem::path p = fsPath(imagePath);
  std::string folder = u8ToString(p.parent_path());
  if (folder.empty())
    folder = ".";
  {
    const auto it = f.byName.find(filmLower(baseName(imagePath)));
    if (it == f.byName.end() || f.files[it->second].path != imagePath)
      filmAdoptFiles({imagePath});
  }
  FilmStrip &g = state.film;
  const auto it = g.byName.find(filmLower(u8ToString(p.filename())));
  g.current = g.target =
      it == g.byName.end() ? -1 : static_cast<int>(it->second);
  if (g.current >= 0) { // the open picture's rating etc. are the editor's own
                        // (loaded with its sidecar)
    FilmEntry &cur = g.files[static_cast<size_t>(g.current)];
    cur.meta = state.meta;
    cur.scanned = true;
  }
  filmRebuildView();
  g.thumbVersion = state.adjust.version();
  filmEnsureVisible(g.current);
  filmQueueThumbs();
  g.dirty = true;
}

// The open picture is about to be replaced: if it was edited, make its
// thumbnail show the edits.
inline void filmLeave() {
  FilmStrip &f = state.film;
  if (f.current < 0 || static_cast<size_t>(f.current) >= f.files.size() ||
      !state.image || state.adjust.version() == f.thumbVersion)
    return;
  FilmEntry &e = f.files[static_cast<size_t>(f.current)];
  if (auto t = makeThumb(*state.image)) {
    e.thumb = std::move(t);
    e.failed = false;
  }
  e.edited = true;
  f.dirty = true;
}

// Called when a sidecar file was written for `path`.
inline void filmMarkEdited(const std::string &path) {
  FilmStrip &f = state.film;
  const auto it = f.byName.find(filmLower(baseName(path)));
  if (it == f.byName.end())
    return;
  FilmEntry &e = f.files[it->second];
  if (!e.edited) {
    e.edited = true;
    f.dirty = true;
  }
}

// A batch rating finished for `path`: the strip shows its new rating, flag and
// label. (If a filter is on, the picture may now appear or disappear.)
inline void filmApplyMeta(const std::string &path, const PhotoMeta &m) {
  FilmStrip &f = state.film;
  const auto it = f.byName.find(filmLower(baseName(path)));
  if (it == f.byName.end() || static_cast<int>(it->second) == f.current)
    return; // the open picture's value is the editor's own
  FilmEntry &e = f.files[it->second];
  e.meta = m;
  e.scanned = true;
  if (f.filter.active())
    filmRebuildView();
  f.dirty = true;
}

// UI thread: takes in the thumbnails the worker has made.
inline void collectThumbJobs() {
  bool metaChanged = false;
  for (auto &r : workers().thumb.collect()) {
    FilmStrip &f = state.film;
    const auto it = f.byName.find(filmLower(baseName(r.path)));
    if (it == f.byName.end() || f.files[it->second].path != r.path)
      continue; // from a folder we have left
    FilmEntry &e = f.files[it->second];
    if (r.thumb) {
      const std::shared_ptr<const CanvasImage> raw = r.thumb;
      // The open picture's own thumbnail already shows its edits: keep that.
      if (!(static_cast<int>(it->second) == f.current &&
            state.adjust.version() != f.thumbVersion))
        e.thumb = std::move(r.thumb);
      // The virtual copies right after it show the same picture until opened.
      if (e.copy == 0)
        for (size_t k = it->second + 1;
             k < f.files.size() && f.files[k].copy > 0; ++k)
          if (!f.files[k].thumb)
            f.files[k].thumb = raw;
    } else if (!r.thumbSkipped)
      e.failed = true; // (a virtual copy is skipped on purpose: not a failure)
    e.edited = e.edited || r.edited;
    if (!e.scanned) { // only the first reading counts: after that the editor's
                      // own value is the
      // truth (a late result could carry what the sidecar said before a rating)
      e.scanned = true;
      e.meta = r.meta;
      metaChanged = metaChanged || !r.meta.isDefault();
    }
    f.dirty = true;
  }
  if (metaChanged && state.film.filter.active())
    filmRebuildView(); // a filter is waiting for these
}

inline void adoptDecoded(DecodeWorker::Result &&r); // defined below

// Opens the picture at index `i` (same background decode as File > Open; a
// virtual copy of the picture that is already open reuses its pixels instead).
inline void filmOpenIndex(int i) {
  FilmStrip &f = state.film;
  if (i < 0 || static_cast<size_t>(i) >= f.files.size() || i == f.target ||
      state.dragging || syncBusy())
    return;
  f.target = i;
  if (f.filter.active())
    filmRebuildView(); // the picture being opened stays in the strip
  f.dirty = true;
  filmEnsureVisible(i);
  const std::string &path = f.files[static_cast<size_t>(i)].path;
  if (state.original && !state.imagePath.empty() && path != state.imagePath &&
      vcImageOf(path) ==
          vcImageOf(state.imagePath)) { // another set of edits for the picture
                                        // on screen: same file, same pixels,
    // so skip the decode and adopt them exactly as a finished decode would
    workers().decode
        .invalidate(); // a decode of some other file is no longer wanted
    DecodeWorker::Result r;
    r.path = path;
    r.image.emplace(*state.original);
    adoptDecoded(
        std::move(r)); // flushes the old copy's edits, loads this one's
    return;
  }
  setStatus("Opening " + f.files[static_cast<size_t>(i)].name + "...", false);
  workers().decode.submit(path);
  filmQueueThumbs();
}

// Left / Right: the previous / next picture (relative to the one asked for
// last, so quick repeats keep moving).
inline void filmStep(int d) {
  const FilmStrip &f = state.film;
  if (f.view.empty())
    return;
  // Steps through the pictures the strip shows (all of them with no filter).
  const int base = f.target >= 0 ? f.target : f.current;
  int pos = filmViewPos(base);
  if (pos < 0)
    pos = d > 0 ? -1 : static_cast<int>(f.view.size());
  pos = std::clamp(pos + d, 0, static_cast<int>(f.view.size()) - 1);
  filmOpenIndex(f.view[static_cast<size_t>(pos)]);
}

inline void filmScrollBy(float dx) {
  state.film.scrollX += dx;
  filmClampScroll();
  state.film.dirty = true;
}

inline void filmClearSelection() {
  for (FilmEntry &e : state.film.files)
    e.selected = false;
  state.film.dirty = true;
}

inline void filmSelectAll() { // only what the strip shows: Sync Settings must
                              // not reach hidden pictures
  for (FilmEntry &e : state.film.files)
    e.selected = false;
  for (int i : state.film.view)
    state.film.files[static_cast<size_t>(i)].selected = true;
  state.film.dirty = true;
}

// ---- The filter bar (top of the strip) ----
// Five stars ("at least N", click the lit one again to switch it off), Pick and
// Reject, five color labels, and "Clear" while a filter is on. Painting and
// clicking both read the same list, so they can't disagree.
inline constexpr float kBarPad = 8.0f, kBarBtn = 22.0f, kBarGap = 14.0f;
inline constexpr float kBarPickW = 38.0f, kBarRejectW = 52.0f, kBarClearW = 44.0f;
inline constexpr std::array<Color, kLabelCount> kLabelColors = {
    Color{225, 70, 70}, Color{232, 196, 60}, Color{80, 185, 100},
    Color{70, 130, 230}, Color{165, 100, 215}};
inline const char *const kLabelDisplay[kLabelCount] = {"Red", "Yellow", "Green",
                                                       "Blue", "Purple"};

struct FilmBarItem {
  enum Kind { None, Star, Pick, Reject, Label, Clear };
  Kind kind = None;
  int n = 0; // the star count or the label number
  float x = 0.0f, w = 0.0f;
};

inline std::vector<FilmBarItem> filmBarItems() {
  std::vector<FilmBarItem> v;
  float x = kBarPad;
  auto add = [&](FilmBarItem::Kind k, int n, float w) {
    FilmBarItem it;
    it.kind = k;
    it.n = n;
    it.x = x;
    it.w = w;
    v.push_back(it);
    x += w;
  };
  for (int s = 1; s <= kMaxRating; ++s)
    add(FilmBarItem::Star, s, kBarBtn);
  x += kBarGap;
  add(FilmBarItem::Pick, 0, kBarPickW);
  add(FilmBarItem::Reject, 0, kBarRejectW);
  x += kBarGap;
  for (int c = 1; c <= kLabelCount; ++c)
    add(FilmBarItem::Label, c, kBarBtn);
  if (state.film.filter.active()) {
    x += kBarGap;
    add(FilmBarItem::Clear, 0, kBarClearW);
  }
  return v;
}

// The filter changed: re-pick what the strip shows. Nothing hidden stays
// selected (Sync Settings would write into it unseen).
inline void filmFilterChanged() {
  FilmStrip &f = state.film;
  filmRebuildView();
  for (size_t i = 0; i < f.files.size(); ++i)
    if (f.files[i].selected && filmViewPos(static_cast<int>(i)) < 0)
      f.files[i].selected = false;
  filmClampScroll();
  filmEnsureVisible(f.current);
  f.dirty = true;
}

inline void filmBarClick(float x) {
  FilmStrip &f = state.film;
  for (const FilmBarItem &it : filmBarItems()) {
    if (x < it.x || x >= it.x + it.w)
      continue;
    switch (it.kind) {
    case FilmBarItem::Star:
      f.filter.toggleRating(it.n);
      break;
    case FilmBarItem::Pick:
      f.filter.toggleFlag(PhotoFlag::Pick);
      break;
    case FilmBarItem::Reject:
      f.filter.toggleFlag(PhotoFlag::Reject);
      break;
    case FilmBarItem::Label:
      f.filter.toggleLabel(it.n);
      break;
    case FilmBarItem::Clear:
      f.filter = MetaFilter{};
      break;
    default:
      return;
    }
    filmFilterChanged();
    return;
  }
}

// A click in the filter bar (y above the cells) or on a picture. A plain click
// on a picture opens it (and clears the selection). Ctrl+click toggles one
// picture in the selection, Shift+click selects the range from the last one
// clicked (among the pictures shown). The open picture joins the selection the
// first time, so it is part of the range.
inline void filmClick(float localX, float localY, const KeyModifiers &mods) {
  FilmStrip &f = state.film;
  if (localY < kFilmBarH) {
    filmBarClick(localX);
    return;
  }
  const int total = static_cast<int>(f.files.size());
  const int n = static_cast<int>(f.view.size());
  const int pos =
      static_cast<int>(std::floor((localX + f.scrollX) / kFilmCellW));
  if (pos < 0 || pos >= n)
    return;
  const int i = f.view[static_cast<size_t>(pos)];
  if (mods.ctrl || mods.shift) {
    const int base = f.current >= 0 ? f.current : i;
    const bool any = std::any_of(f.files.begin(), f.files.end(),
                                 [](const FilmEntry &e) { return e.selected; });
    if (!any)
      f.files[static_cast<size_t>(base)].selected = true;
    if (mods.shift) {
      const int a = f.anchor >= 0 && f.anchor < total ? f.anchor : base;
      int pa = filmViewPos(a);
      if (pa < 0)
        pa = filmViewPos(base);
      if (pa < 0)
        pa = pos;
      for (FilmEntry &e : f.files)
        e.selected = false;
      for (int k = std::min(pa, pos); k <= std::max(pa, pos); ++k)
        f.files[static_cast<size_t>(f.view[static_cast<size_t>(k)])].selected =
            true;
    } else {
      FilmEntry &e = f.files[static_cast<size_t>(i)];
      e.selected = !e.selected;
      f.anchor = i;
    }
    f.dirty = true;
    return;
  }
  filmClearSelection();
  f.anchor = i;
  filmOpenIndex(i);
}

inline void toggleFilmstrip() {
  state.film.visible = !state.film.visible;
  state.film.dirty = true;
}

inline bool filmstripShown() {
  return state.film.visible && !state.film.files.empty();
}

// File > Open Folder: lists the folder and opens its first picture.
inline void openFolder() {
  if (syncBusy())
    return;
  auto dir = openFolderPicker("Open Folder");
  if (!dir)
    return; // user cancelled
  if (!filmAdoptFolder(*dir)) {
    setStatus("No JPEG or PNG pictures in " + baseName(*dir), true);
    return;
  }
  filmQueueThumbs();
  filmOpenIndex(0);
}

inline std::string starsText(int n) {
  std::string s;
  for (int i = 0; i < n; ++i)
    s += "\xE2\x98\x85"; // U+2605 BLACK STAR
  return s;
}

// The texture paintFilmstrip draws for `e`: built from e.thumb the first time
// and again whenever e.thumb is replaced (the worker delivers a thumbnail, or
// the open picture's own is refreshed with its edits). Every thumbnail keeps
// its own GPU copy across repaints. drawImage has ONE cache slot per canvas,
// so drawing a row of thumbnails with it re-uploaded every visible one on
// every repaint (scroll, hover, selection). thumbTexOf holds the source image
// so its address cannot be reused while it is compared. Null if there is no
// thumbnail or the texture could not be made.
inline const std::shared_ptr<CanvasTexture> &filmThumbTexture(const FilmEntry &e) {
  if (e.thumbTexOf != e.thumb) {
    e.thumbTex.reset();
    e.thumbTexOf = e.thumb;
    if (e.thumb && e.thumb->width > 0 && e.thumb->height > 0) {
      auto t = std::make_shared<CanvasTexture>(e.thumb->width, e.thumb->height);
      if (t->update(0, 0, *e.thumb))
        e.thumbTex = std::move(t);
    }
  }
  return e.thumbTex;
}

inline void paintFilmstrip(CanvasContext &ctx) {
  FilmStrip &f = state.film;
  const float W = ctx.width();
  f.viewW = W;
  filmClampScroll();
  ctx.setFillColor(kColorRootBg);
  ctx.fillRect(0, 0, W, kFilmH);
  if (f.files.empty())
    return;

  // ---- filter bar ----
  {
    const MetaFilter &fl = f.filter;
    const Color gold{255, 205, 60};
    ctx.setFont("", 12.0f);
    ctx.setTextBaseline(TextBaseline::Top);
    ctx.setTextAlign(TextAlign::Center);
    for (const FilmBarItem &it : filmBarItems()) {
      const float cx = it.x + it.w / 2.0f;
      bool on = false;
      switch (it.kind) {
      case FilmBarItem::Star:
        on = it.n <= fl.minRating;
        break;
      case FilmBarItem::Pick:
        on = (fl.flags & (1u << static_cast<unsigned>(PhotoFlag::Pick))) != 0;
        break;
      case FilmBarItem::Reject:
        on = (fl.flags & (1u << static_cast<unsigned>(PhotoFlag::Reject))) != 0;
        break;
      case FilmBarItem::Label:
        on = (fl.labels & (1u << static_cast<unsigned>(it.n))) != 0;
        break;
      default:
        break;
      }
      if (on && it.kind != FilmBarItem::Star) {
        ctx.setFillColor(kColorTrack);
        ctx.fillRect(it.x, 3, it.w, kFilmBarH - 6);
      }
      switch (it.kind) {
      case FilmBarItem::Star:
        ctx.setFillColor(on ? gold : kColorTextFaint);
        ctx.fillText("\xE2\x98\x85", cx, 6);
        break;
      case FilmBarItem::Pick:
        ctx.setFillColor(on ? kColorTextDark : kColorTextMuted);
        ctx.fillText("Pick", cx, 6);
        break;
      case FilmBarItem::Reject:
        ctx.setFillColor(on ? kColorTextDark : kColorTextMuted);
        ctx.fillText("Reject", cx, 6);
        break;
      case FilmBarItem::Label: {
        Color c = kLabelColors[static_cast<size_t>(it.n - 1)];
        if (!on)
          c.a = 140;
        ctx.setFillColor(c);
        ctx.beginPath();
        ctx.roundRect(cx - 6, 7, 12, 12, 6);
        ctx.fill();
        break;
      }
      case FilmBarItem::Clear:
        ctx.setFillColor(kColorAccent);
        ctx.fillText("Clear", cx, 6);
        break;
      default:
        break;
      }
    }
    if (fl.active()) {
      size_t unread = 0;
      for (const FilmEntry &e : f.files)
        unread += e.scanned ? 0 : 1;
      std::string t = std::to_string(f.view.size()) + " of " +
                      std::to_string(f.files.size());
      if (unread > 0)
        t += " (reading ratings\u2026)"; // thumbnails bring each picture's
                                         // rating in
      ctx.setTextAlign(TextAlign::End);
      ctx.setFillColor(kColorTextMuted);
      ctx.fillText(t, W - kBarPad, 6);
    }
  }

  // ---- cells ----
  const float oy = kFilmBarH, H = kFilmCellsH;
  const int n = static_cast<int>(f.view.size());
  if (n == 0) {
    ctx.setFont("", 12.0f);
    ctx.setTextAlign(TextAlign::Center);
    ctx.setTextBaseline(TextBaseline::Top);
    ctx.setFillColor(kColorTextMuted);
    ctx.fillText("No photos match the filter", W / 2.0f, oy + H / 2.0f - 6);
    return;
  }
  const int first = std::max(0, static_cast<int>(f.scrollX / kFilmCellW));
  const int last =
      std::min(n - 1, static_cast<int>((f.scrollX + W) / kFilmCellW));
  for (int p = first; p <= last; ++p) {
    const int i = f.view[static_cast<size_t>(p)];
    const FilmEntry &e = f.files[static_cast<size_t>(i)];
    const float cx = static_cast<float>(p) * kFilmCellW - f.scrollX;
    const bool selected = i == f.target;
    if (selected) {
      ctx.setFillColor(kColorActivityActive);
      ctx.fillRect(cx + 2, oy + 2, kFilmCellW - 4, H - 4);
    } else if (e.selected) { // picked for Sync Settings
      ctx.setFillColor(kColorTrack);
      ctx.fillRect(cx + 2, oy + 2, kFilmCellW - 4, H - 4);
      ctx.setStrokeColor(kColorAccent);
      ctx.setLineWidth(1.0f);
      ctx.strokeRect(cx + 2, oy + 2, kFilmCellW - 4, H - 4);
    }
    const float bx = cx + (kFilmCellW - kFilmThumbW) / 2.0f, by = oy + 6.0f;
    if (e.thumb) {
      const float x = bx + (kFilmThumbW - e.thumb->width) / 2.0f;
      const float y = by + (kFilmThumbH - e.thumb->height) / 2.0f;
      if (const auto &tex = filmThumbTexture(e))
        ctx.drawTexture(tex, std::floor(x), std::floor(y));
      else
        ctx.drawImage(*e.thumb, std::floor(x), std::floor(y)); // fallback
    } else {
      ctx.setFillColor(kColorTrack);
      ctx.fillRect(bx, by, kFilmThumbW, kFilmThumbH);
    }
    if (e.meta.flag ==
        PhotoFlag::Reject) { // dimmed, like a rejected frame on a contact sheet
      ctx.setFillColor(Color{0, 0, 0, 130});
      ctx.fillRect(bx, by, kFilmThumbW, kFilmThumbH);
    }
    if (selected) {
      ctx.setStrokeColor(kColorAccent);
      ctx.setLineWidth(2.0f);
      ctx.strokeRect(cx + 2, oy + 2, kFilmCellW - 4, H - 4);
    }
    if (e.edited) { // a dot in the thumbnail's bottom-right corner
      ctx.setFillColor(kColorAccent);
      ctx.beginPath();
      ctx.roundRect(bx + kFilmThumbW - 12, by + kFilmThumbH - 12, 8, 8, 4);
      ctx.fill();
    }
    if (e.copy >
        0) { // "C2": this cell is virtual copy 2 of the picture before it
      ctx.setFillColor(Color{0, 0, 0, 150});
      ctx.fillRect(bx + kFilmThumbW - 36, by + kFilmThumbH - 15, 20, 13);
      ctx.setFont("", 9.0f);
      ctx.setTextAlign(TextAlign::Center);
      ctx.setTextBaseline(TextBaseline::Top);
      ctx.setFillColor(Color{255, 255, 255});
      ctx.fillText("C" + std::to_string(e.copy), bx + kFilmThumbW - 26,
                   by + kFilmThumbH - 14);
    }
    if (e.meta.rating > 0) { // gold stars on the bottom-left corner
      ctx.setFillColor(Color{0, 0, 0, 150});
      ctx.fillRect(bx + 2, by + kFilmThumbH - 15, 7.0f + 9.0f * e.meta.rating,
                   13);
      ctx.setFont("", 9.0f);
      ctx.setTextAlign(TextAlign::Start);
      ctx.setTextBaseline(TextBaseline::Top);
      ctx.setFillColor(Color{255, 205, 60});
      ctx.fillText(starsText(e.meta.rating), bx + 5, by + kFilmThumbH - 14);
    }
    if (e.meta.label > 0) { // color dot, top-left
      ctx.setFillColor(Color{0, 0, 0, 150});
      ctx.beginPath();
      ctx.roundRect(bx + 3, by + 3, 14, 14, 7);
      ctx.fill();
      ctx.setFillColor(kLabelColors[static_cast<size_t>(e.meta.label - 1)]);
      ctx.beginPath();
      ctx.roundRect(bx + 5, by + 5, 10, 10, 5);
      ctx.fill();
    }
    if (e.meta.flag != PhotoFlag::None) { // "P" / "X" badge, top-right
      const bool pick = e.meta.flag == PhotoFlag::Pick;
      ctx.setFillColor(pick ? Color{70, 165, 90} : Color{205, 60, 60});
      ctx.beginPath();
      ctx.roundRect(bx + kFilmThumbW - 17, by + 3, 14, 14, 3);
      ctx.fill();
      ctx.setFont("", 10.0f);
      ctx.setTextAlign(TextAlign::Center);
      ctx.setTextBaseline(TextBaseline::Top);
      ctx.setFillColor(Color{255, 255, 255});
      ctx.fillText(pick ? "P" : "X", bx + kFilmThumbW - 10, by + 4);
    }
    std::string label = e.name;
    constexpr size_t kMaxChars = 15;
    if (label.size() > kMaxChars) {
      size_t cut = kMaxChars - 1; // never split a UTF-8 character
      while (cut > 0 && (static_cast<unsigned char>(label[cut]) & 0xC0) == 0x80)
        --cut;
      label = label.substr(0, cut) + "\u2026";
    }
    ctx.setFont("", 11.0f);
    ctx.setTextAlign(TextAlign::Center);
    ctx.setTextBaseline(TextBaseline::Top);
    ctx.setFillColor(selected ? kColorTextDark : kColorTextMuted);
    ctx.fillText(label, cx + kFilmCellW / 2.0f, by + kFilmThumbH + 6.0f);
  }
}
// END filmstrip