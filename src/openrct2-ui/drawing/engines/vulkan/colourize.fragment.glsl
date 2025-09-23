#version 450
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(1u << 22u);//1.0 / float(1u << 22u);

layout (set = 0, binding = 0) uniform sampler singleSampler;

layout (set = 0, binding = 1) uniform utexture2D filterPalette;
layout (set = 0, binding = 2) uniform utexture2D blendPalette;

layout (input_attachment_index = 0, set = 1, binding = 0) uniform usubpassInput inputColour;
layout (input_attachment_index = 2, set = 1, binding = 1) uniform subpassInput inputDepth;

layout (set = 1, binding = 2, std430) uniform globals{
    uint colourPalette[256]; //palette to BGRA colour mapping
};

layout(push_constant) uniform pushConstants
{
    uint rectCount;
} push;

const uint FLAGS_ACTION_MASK = 0x1;

const uint FLAGS_ACTION_FILTERRECT = 0x0;
const uint FLAGS_ACTION_BLEND = 0x1;

struct FilterRect
{
    ivec4 bounds;
    ivec4 clip;
	uint flags;
    uint filterId; //filter number to use for filterPalette
    uint depth;
};

layout (set = 1, binding = 3, std430) readonly buffer FilterRects {
    FilterRect[] rects;
};

layout (location = 1) out vec3 outColour;

void main() {
    uint colour = subpassLoad(inputColour).x;
    float depth = subpassLoad(inputDepth).x;
    vec2 coords = gl_FragCoord.xy;

    for(uint i = 0; i < push.rectCount; i++)
    {
        if(depth < (rects[i].depth * DEPTH_INCREMENT))
        {
            if(
                coords.x > rects[i].bounds.x &&
                coords.y > rects[i].bounds.y &&
                coords.x < rects[i].bounds.z &&
                coords.y < rects[i].bounds.w)
            {
				if((rects[i].flags & FLAGS_ACTION_MASK) == FLAGS_ACTION_FILTERRECT)
				{
					//vec4 clipRect = rects[i].clip * push.scaleFactor;
					ivec2 uv = ivec2(colour, rects[i].filterId);
					uint thisColour = texelFetch(usampler2D(filterPalette, singleSampler), uv, 0).x;
					if(thisColour != 0)
					{
						colour = thisColour;
					}
				}
				else
				{
					// temporarily empty
				}
            }
        }
    }

    uint rawBGRA = colourPalette[colour];

    // alpha = (rawBGRA & 0xFF000000) >> 24;
    outColour = vec3(
        (rawBGRA & 0xFF0000) >> 16,
        (rawBGRA & 0xFF00) >> 8,
        (rawBGRA & 0xFF) >> 0
        ) / 255.0;
}
