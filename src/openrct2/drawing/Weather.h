/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <array>
#include <cstdint>

struct RenderTarget;

namespace OpenRCT2::Drawing
{
    struct IWeatherDrawer;
}

struct WeatherPatternDataEntry
{
    uint8_t PatternX;
    uint8_t Colour;
    constexpr WeatherPatternDataEntry(uint8_t patternX, uint8_t colour) : PatternX(patternX), Colour(colour)
    {

    }
};

struct WeatherPatternData
{
    uint8_t SizeX;
    uint8_t SizeY;
    std::array<WeatherPatternDataEntry, 32> Data;
    constexpr WeatherPatternData(uint8_t sizeX, uint8_t sizeY, std::array<WeatherPatternDataEntry, 32> data) : SizeX(sizeX), SizeY(sizeY), Data(data)
    {

    }
};

static constexpr uint8_t kRainPatternSizeX = 32;
static constexpr uint8_t kRainPatternSizeY = 32;

// clang-format off
static constexpr std::array<WeatherPatternDataEntry, kRainPatternSizeY> kRainPatternData =
{{
    {0, 12}, {0, 14}, {0, 16}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255,
    0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0},
    {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255,
    0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}
}};
// clang-format on

static constexpr WeatherPatternData kRainPattern(kRainPatternSizeX, kRainPatternSizeY, kRainPatternData);

static constexpr uint32_t kSnowPatternSizeX = 32;
static constexpr uint32_t kSnowPatternSizeY = 32;

// clang-format off
static constexpr std::array<WeatherPatternDataEntry, kSnowPatternSizeY> kSnowPatternData =
{{
    {0, 32}, {0, 32}, {0, 16}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255,
    0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0},
    {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255,
    0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}, {255, 0}
}};
// clang-format on

static constexpr WeatherPatternData kSnowPattern(kSnowPatternSizeX, kSnowPatternSizeY, kSnowPatternData);

void DrawWeather(RenderTarget& rt, OpenRCT2::Drawing::IWeatherDrawer* weatherDrawer);
