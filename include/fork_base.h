#pragma once

/* How we decided for the fork */
enum class fork_reason_t {
  reason_long,    // key pressed too long
  reason_overlap, // key press overlaps with another key
  reason_force,   // mouse-button was pressed & triggered fork.
  reason_short,
  reason_wrong,
};

