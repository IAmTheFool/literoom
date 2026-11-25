// app/sync_busy.hpp: true (with a status message) while a sync is running
#pragma once

#include "app/editor_state.hpp" // setStatus
#include "app/workers.hpp"      // workers()

inline bool syncBusy() {
  if (!workers().sync.outstanding())
    return false;
  setStatus("Wait for the sync to finish", true);
  return true;
}