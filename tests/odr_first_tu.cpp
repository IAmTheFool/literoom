// tests/odr_first_tu.cpp - the FIRST translation unit of the ODR check
// (literoom_odr_test, see CMakeLists.txt): src/main.cpp compiled into the test
// with its main() renamed, like golden_render_test.cpp and lut_pane_test.cpp do,
// plus the real main() of this executable.
//
// The test never starts the editor. It asks both translation units for the
// addresses of the app's shared objects and functions (tests/odr_probe.hpp) and
// fails if any differ: that is a `static` that came back into an app header.
// (A definition that is neither `static` nor `inline` does not get this far: it
// is a "multiple definition" link error.)
#define LITEUI_IMPLEMENTATION
#include "liteui.hpp"
#include "mask/sam_segmenter.hpp"
#include "io/exif.hpp"
#include "io/minijson.hpp"
#include "detail/detail_layer.hpp"
#include "detail/neighbor_fx.hpp"
// main.cpp's own includes, pulled in first so that the `main` macro below
// renames only main.cpp's main().
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>
#define main literoom_app_main_first_tu
#include "main.cpp"
#undef main

#include "odr_probe.hpp"

static OdrProbe odrProbeFirstTu() {
  return OdrProbe{&state,
                  &workers(),
                  &gLuts,
                  &exportRun,
                  &kCropAspects,
                  reinterpret_cast<void (*)()>(&setStatus),
                  reinterpret_cast<void (*)()>(&currentLutStatus)};
}

int main() {
  const OdrProbe a = odrProbeFirstTu();
  const OdrProbe b = odrProbeSecondTu();

  int bad = 0;
  const auto check = [&bad](const char *what, bool same) {
    std::printf("%-22s %s\n", what, same ? "one object" : "TWO COPIES");
    if (!same)
      ++bad;
  };
  check("editor state", a.state == b.state);
  check("Workers", a.workers == b.workers);
  check("LUT list (gLuts)", a.luts == b.luts);
  check("export run", a.exportRun == b.exportRun);
  check("crop aspects table", a.cropAspects == b.cropAspects);
  check("setStatus()", a.setStatusFn == b.setStatusFn);
  check("currentLutStatus()", a.lutStatusFn == b.lutStatusFn);

  if (bad != 0) {
    std::fprintf(stderr,
                 "\nODR check failed: %d shared thing(s) exist once per .cpp. "
                 "Something in src/app/ is `static` again: make it `inline` "
                 "(tools/inlinify_headers.py).\n",
                 bad);
    return 1;
  }
  return 0;
}
