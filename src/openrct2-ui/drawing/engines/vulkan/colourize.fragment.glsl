#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout (set = 0, binding = 0) uniform sampler singleSampler;

layout (set = 0, binding = 1) uniform utexture2D filterPalette;

layout (input_attachment_index = 0, set = 1, binding = 0) uniform usubpassInput inputColour;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform usubpassInput inputDepth;

layout (set = 1, binding = 2, std430) uniform globals{
    uint colourPalette[256]; //palette to BGRA colour mapping
};

layout(push_constant) uniform pushConstants
{
    uint rectCount;
    float scaleFactor;
} push;

struct FilterRect
{
    ivec4 bounds;
    uint depth;
    uint filterId; //filter number to use for filterPalette
};

layout (set = 1, binding = 3) readonly buffer FilterRects {
    FilterRect rects;
};

layout (location = 2) out vec3 outColour;

void main() {
    uint colour = subpassLoad(inputColour).x;
    uint depth = subpassLoad(inputDepth).x;

    uvec3 intColour = uvec3(
        (colourPalette[colour] & 0xFF0000) >> 16,
        (colourPalette[colour] & 0xFF00) >> 8,
        colourPalette[colour] & 0xFF);

    outColour = vec3(intColour) / 255.0;

    // This will likely end up something like below
    // 
    // uint colour = subpassLoad(inputColour);
    // uint depth = subpassLoad(inputDepth).x;
    // ivec2 coords = ???gl_FragCoord.xy???;
    //
    // for(uint i = 0; i< global.rectCount; i++)
    // {
    //     if(depth < bounds.depth && coords.x > bounds.x && coord.y > bounds.y && coords.x < bounds.z && coords.y < bounds.w)
    //     {
    //         colour = texture(singleSampler, filterPalette, rects[i].filterId)
    //     }
    // }
    //
    // outColour = texture(singleSampler, colourFilter, colourPalette[colour]);
}
