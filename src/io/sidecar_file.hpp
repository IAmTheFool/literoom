// io/sidecar_file.hpp - reading a picture's sidecar file from disk.
//
// The ONE place that decides what "the sidecar" of a picture is on disk: which
// file (sidecarFileOf, through fsPath so a non-ASCII path survives on Windows),
// whether it is absent or merely unreadable, and how big a file may be. The
// editor (loadSidecar), Sync Settings, the filmstrip's ThumbWorker and the
// batch export all read through readSidecarFile, so a fix here reaches all
// four. What the text MEANS is sidecar.hpp's job (sidecarCheckText, then
// sidecarEditsFromJson / AdjustmentSet::loadJson); what to do about a bad one is
// each caller's policy (the editor opens such a picture clean, export skips it).
//
// An existing file that can't be opened is Unreadable, never Absent: treating
// it as absent would let a caller rebuild the sidecar from nothing and replace
// the real file (losing crop, rotation and masks).
#pragma once
#include "io/file_utils.hpp" // fsPath
#include "io/sidecar.hpp"    // sidecarFileOf

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

// Largest sidecar anyone reads. (minijson's own input limit is the same 64 MiB.)
constexpr std::streamoff kMaxSidecarBytes = std::streamoff{64} << 20;

enum class SidecarRead {
  Absent,     // no such file: normal for a picture nobody has touched
  Ok,         // *text holds the file
  Empty,      // the file is there and holds nothing (nothing to lose)
  TooBig,     // over kMaxSidecarBytes
  Unreadable, // there, but can't be checked, opened or read
};

// The sidecar's file as a filesystem path (UTF-8 safe). The ONE definition of
// where a picture's sidecar lives: readSidecarFile reads it, and every writer
// (the editor's saveSidecar, Sync Settings) writes it, through this, so reading
// and writing can't disagree.
inline std::filesystem::path sidecarFilePath(const std::string &imagePath) {
  return fsPath(sidecarFileOf(imagePath));
}

// Reads `imagePath`'s sidecar (a virtual copy's path reads that copy's own
// file). *text is only filled for Ok. For TooBig and Unreadable, *why (if
// given) gets a short reason for a status line. Any thread.
inline SidecarRead readSidecarFile(const std::string &imagePath,
                                   std::string *text,
                                   std::string *why = nullptr) {
  const auto fail = [&](SidecarRead r, const char *reason) {
    if (why)
      *why = reason;
    return r;
  };
  const std::filesystem::path p = sidecarFilePath(imagePath);
  std::error_code ec;
  const bool exists = std::filesystem::exists(p, ec);
  if (ec)
    return fail(SidecarRead::Unreadable,
                "couldn't check the file"); // can't tell: don't guess "absent"
  if (!exists)
    return SidecarRead::Absent;
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f)
    return fail(SidecarRead::Unreadable, "couldn't open the file");
  const std::streamoff n = f.tellg();
  if (n < 0)
    return fail(SidecarRead::Unreadable, "unexpected file size");
  if (n == 0)
    return SidecarRead::Empty;
  if (n > kMaxSidecarBytes)
    return fail(SidecarRead::TooBig, "unexpected file size");
  std::string buf(static_cast<size_t>(n), '\0');
  f.seekg(0);
  f.read(buf.data(), n);
  if (!f)
    return fail(SidecarRead::Unreadable, "couldn't read the file");
  *text = std::move(buf);
  return SidecarRead::Ok;
}
