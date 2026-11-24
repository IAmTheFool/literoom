// tests/odr_probe.hpp - what the ODR check compares between its two translation
// units (see the literoom_odr_test block in CMakeLists.txt).
//
// Each unit fills one of these with the addresses of shared things in the app
// layer. If they are one object program-wide (`inline`), both units report the
// same addresses; a `static` gives each unit its own copy, so they differ.
//
// Add a field here (and to both tests/odr_*_tu.cpp) when a new shared object
// deserves a guard. The names below are the app's own: if one is renamed, the
// test stops compiling, which is the cue to update it.
#pragma once

struct OdrProbe {
  const void *state;       // the editor state
  const void *workers;     // the one Workers (all eight background threads)
  const void *luts;        // gLuts: a header-level vector in lut_pane.hpp
  const void *exportRun;   // exportRun: a header-level struct in export.hpp
  const void *cropAspects; // kCropAspects: a header-level const table
  void (*setStatusFn)();   // a function: inline functions have ONE address
  void (*lutStatusFn)();   // a second one, whose body holds function-local statics
};

// Defined in tests/odr_second_tu.cpp; the first unit calls it.
OdrProbe odrProbeSecondTu();
