#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require

const uint FLAG_COLOR_ONLY = 1;
const uint FLAG_MASK_SELF = 2;

layout(set = 0, binding = 1) uniform sampler singleSampler;
layout(set = 0, binding = 2) uniform DrawInfo
{
    vec4 palette[256];
} drawInfo;

layout(set = 1, binding = 0) uniform utexture2D textures[];

layout(location = 0) flat in uint textureIndex;
layout(location = 1) in vec2 texCoord;
layout(location = 2) flat in uint flags;

layout(location = 0) out vec4 outColor;

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
    
    if((flags & FLAG_MASK_SELF) != 0 && upaletteindex == 0)
    {
        // This sprite uses 0 as transparent
        discard;
    }

    outColor = drawInfo.palette[upaletteindex];
}
