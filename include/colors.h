#pragma once

# if USE_COLORS
// todo:

// global variables:
// typedef const char* color;
using color=const char*;

const color escape_sequence="\x1b";
const color info_color = "\x1b[47;31m";
const color fork_color="\x1b[47;30m";
const color key_color="\x1b[01;32;41m";
const color keysym_color="\x1b[33;01m";
const color warning_color="\x1b[38;5;160m";
const color key_io_color="\x1b[38;5;226m";
const color color_reset="\x1b[0m";

# else // USE_COLORS
#define info_color ""
#define key_color  ""
#define warning_color  ""
#define color_reset ""
#define escape_sequence ""
// ....
#endif // USE_COLORS
