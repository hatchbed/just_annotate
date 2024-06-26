#pragma once

#include <cstdint>
#include <array>

const std::array<std::array<uint8_t, 3>, 7> PLOT_COLORS = {{
    {0xE6, 0xB3, 0x00},  // yellow
    {0x00, 0x51, 0xE6},  // blue
    {0xE6, 0x00, 0x77},  // magenta
    {0x00, 0xE6, 0x5D},  // green
    {0xE6, 0x73, 0x00},  // orange
    {0x82, 0x00, 0xE6},  // purple
    {0x00, 0xC9, 0xE6}   // cyan
}};
