// tests/odr_second_tu.cpp - the SECOND translation unit of the ODR check
// (literoom_odr_test, see CMakeLists.txt). It includes src/main.cpp exactly like
// tests/odr_first_tu.cpp does, so it sees the same app headers in the same order
// with no include list to keep in sync, but it must not compile the liteui
// implementation a second time: LITEROOM_NO_LITEUI_IMPL tells main.cpp so.
#define LITEROOM_NO_LITEUI_IMPL
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

#define main literoom_app_main_second_tu
#include "main.cpp"
#undef main

#include "odr_probe.hpp"

OdrProbe odrProbeSecondTu() {
  return OdrProbe{&state,
                  &workers(),
                  &gLuts,
                  &exportRun,
                  &kCropAspects,
                  reinterpret_cast<void (*)()>(&setStatus),
                  reinterpret_cast<void (*)()>(&currentLutStatus)};
}