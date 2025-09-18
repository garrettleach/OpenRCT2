#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require

const uint PALETTE_COUNT_MASK = 0x3;

const uint FLAG_COLOR_ONLY = 1;
const uint FLAG_MASK = 2;
const uint FLAG_CROSS_HATCH = 4;

const uint kPaletteOffsetRemapPrimary = 243; // if secondary also did not apply then if we have at least 1 palette use the palette color as-is
const uint kPaletteOffsetRemapSecondary = 202; // if tertiary did not apply and we have at least 2 palette and the second is in the range [202,202+12) remap to 243+[0,12)
const uint kPaletteOffsetRemapTertiary = 46; // when we have a 3 palette image and the third falls in [46, 46+12) then remap to 243+[0,12)
const uint kPaletteLengthRemap = 12;

const uint kPaletteExclusiveEndRemapPrimary = kPaletteOffsetRemapPrimary + kPaletteLengthRemap;
const uint kPaletteExclusiveEndRemapSecondary = kPaletteOffsetRemapSecondary + kPaletteLengthRemap;
const uint kPaletteExclusiveEndRemapTertiary = kPaletteOffsetRemapTertiary+ kPaletteLengthRemap;

const uint kPaletteAddRemapSecondary = kPaletteOffsetRemapPrimary - kPaletteOffsetRemapSecondary;
const uint kPaletteAddRemapTertiary = kPaletteOffsetRemapPrimary - kPaletteOffsetRemapTertiary;

layout(set = 0, binding = 1) uniform sampler singleSampler;
layout(set = 0, binding = 2) uniform utexture2D filterPalette;

layout(set = 1, binding = 0) uniform utexture2D textures[];

layout(location = 0) flat in uint flags;
layout(location = 1) flat in uint textureIndex;
layout(location = 2) flat in uint maskIndex;
layout(location = 3) flat in uint remapPalette;// 0xFF Primary, 0xFF00 Seconday, 0xFF0000 Tertiary, 0x3000000 count of palettes
layout(location = 4) in vec2 texCoord;
layout(location = 5) in vec2 texCoordUnnormalized;

layout(location = 0) out uint outColour;

void main() {
    uint upaletteindex;

    if((flags & FLAG_COLOR_ONLY) != 0)
    {
        upaletteindex = textureIndex;
    }
    else
    {
        upaletteindex = texture(usampler2D(textures[textureIndex], singleSampler), texCoord).r;
    }

    uint paletteCount = (remapPalette >> 24) & PALETTE_COUNT_MASK;
    if(paletteCount == 3 &&
        upaletteindex >= kPaletteOffsetRemapTertiary &&
        upaletteindex < kPaletteExclusiveEndRemapTertiary)
    {
        upaletteindex = texelFetch(usampler2D(filterPalette, singleSampler), ivec2(upaletteindex + kPaletteAddRemapTertiary, (remapPalette >> 16) & 0xFF), 0).r;
    }
    else if(paletteCount >= 2 &&
        upaletteindex >= kPaletteOffsetRemapSecondary &&
        upaletteindex < kPaletteExclusiveEndRemapSecondary)
    {
        upaletteindex = texelFetch(usampler2D(filterPalette, singleSampler), ivec2(upaletteindex + kPaletteAddRemapSecondary, (remapPalette >> 8) & 0xFF), 0).r;
    }
    else if(paletteCount >= 1)
    {
        upaletteindex = texelFetch(usampler2D(filterPalette, singleSampler), ivec2(upaletteindex, remapPalette & 0xFF), 0).r;
    }

    if(upaletteindex == 0)
    {
        discard;
    }

    if((flags & FLAG_CROSS_HATCH) != 0)
    {
        vec2 position = vec2(texCoordUnnormalized);
        int posSum = int(position.x) + int(position.y);
        if ((posSum % 2) != 0)
        {
            discard;
        }
    }
    if((flags & FLAG_MASK) != 0)
    {
        uint maskValue = texture(usampler2D(textures[maskIndex], singleSampler), texCoord).r;

        if(maskValue == 0)
        {
            discard;
        }
    }

    outColour = upaletteindex;
}
