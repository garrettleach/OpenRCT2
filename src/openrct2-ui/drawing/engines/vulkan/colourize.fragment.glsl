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

const uint FLAGS_ACTION_MASK = 0x3;

const uint FLAGS_ACTION_FILTERRECT = 0x0;
const uint FLAGS_ACTION_BLEND_WITH_PALETTE = 0x1;
const uint FLAGS_ACTION_BLEND_WITH_EXISTING = 0x2;

struct ColourizeCommand
{
    ivec4 bounds;
    ivec4 clip;
	uint flags;
    int zoom;
	// filterId:
	//   in FLAGS_ACTION_FILTERRECT: filter palette id
	//   in FLAGS_ACTION_BLEND_WITH_PALETTE: value to blend previous colour with
	//   in FLAGS_ACTION_BLEND_WITH_EXISTING: offset to use when determining filter to use
    uint filterId; //filter number to use for filterPalette and dstColor in blend
    uint depth;
	uint textureIndex;
};

layout (set = 1, binding = 3, std430) readonly buffer FilterRects {
    ColourizeCommand[] commands;
};

layout (set = 2, binding = 0) uniform utexture2D textures[];

layout (location = 1) out vec3 outColour;

void main() {
    uint colour = subpassLoad(inputColour).x;
    float depth = subpassLoad(inputDepth).x;
    vec2 coords = gl_FragCoord.xy;

    for(uint i = 0; i < push.rectCount; i++)
    {
        if(depth < (commands[i].depth * DEPTH_INCREMENT))
        {
            if(
                coords.x > commands[i].bounds.x &&
                coords.y > commands[i].bounds.y &&
                coords.x < commands[i].bounds.z &&
                coords.y < commands[i].bounds.w &&
				coords.x > commands[i].clip.x &&
                coords.y > commands[i].clip.y &&
                coords.x < commands[i].clip.z &&
                coords.y < commands[i].clip.w )
            {
				if((commands[i].flags & FLAGS_ACTION_MASK) == FLAGS_ACTION_FILTERRECT) // GENERALLY GUI
				{
					//vec4 clipRect = commands[i].clip * push.scaleFactor;
					ivec2 uv = ivec2(colour, commands[i].filterId);
					uint thisColour = texelFetch(usampler2D(filterPalette, singleSampler), uv, 0).x;
					if(thisColour != 0)
					{
						colour = thisColour;
					}
				}
				else if((commands[i].flags & FLAGS_ACTION_MASK) == FLAGS_ACTION_BLEND_WITH_EXISTING) // EXAMPLE: WATER
				{
					ivec2 uvTexture = ivec2(coords.x - commands[i].bounds.x, coords.y - commands[i].bounds.y);
					if(commands[i].zoom > 0)
					{
						uvTexture = uvTexture << commands[i].zoom;
					}
					else if(commands[i].zoom < 0)
					{
						uvTexture = uvTexture >> -commands[i].zoom;
					}
                    uint colourInSprite = texelFetch(usampler2D(textures[commands[i].textureIndex], singleSampler), uvTexture, 0).x;
					
					if(colourInSprite != 0)
					{
						ivec2 uvColourToFilter = ivec2(colour, colourInSprite+commands[i].filterId-1);
						
						uint possibleWater = texelFetch(usampler2D(filterPalette, singleSampler), uvColourToFilter, 0).x;
					
						if(possibleWater != 0)
						{
							colour = possibleWater;
						}
					}
				}
				else
				{
					// FLAGS_ACTION_BLEND_WITH_PALETTE EXAMPLE: GLASS (texture is used only as a mask)
				
					ivec2 uvMaskTexture = ivec2(coords.x - commands[i].bounds.x, coords.y - commands[i].bounds.y);
					if(commands[i].zoom > 0)
					{
						uvMaskTexture = uvMaskTexture << commands[i].zoom;
					}
					else if(commands[i].zoom < 0)
					{
						uvMaskTexture = uvMaskTexture >> -commands[i].zoom;
					}
					
                    uint maskValue = texelFetch(usampler2D(textures[commands[i].textureIndex], singleSampler), uvMaskTexture, 0).x;
				
					if(maskValue != 0)
					{
						ivec2 uvBlend = ivec2(colour, commands[i].filterId);
						uint thisColour = texelFetch(usampler2D(filterPalette, singleSampler), uvBlend, 0).x;
						if(thisColour != 0)
						{
							colour = thisColour;
						}
					}
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
