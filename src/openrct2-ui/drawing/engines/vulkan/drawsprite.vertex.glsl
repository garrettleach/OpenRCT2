#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(1u << 22u);//1.0 / float(1u << 22u);

layout(push_constant) uniform pushConstants
{
    uvec2 renderTargetSize;
} push;

layout(location = 0) in ivec4 bounds;
layout(location = 1) in ivec4 clip;
layout(location = 2) in uint flags;
layout(location = 3) in uint index;
layout(location = 4) in uint maskIndex;
layout(location = 5) in uint remapPalette;
layout(location = 6) in ivec2 inPosition;


layout(location = 0) flat out uint outFlags;
layout(location = 1) flat out uint outTextureIndex;
layout(location = 2) flat out uint outMaskIndex;
layout(location = 3) flat out uint outRemapPalette;
layout(location = 4) out vec2 outTexCoord;
layout(location = 5) out vec2 outTexCoordUnnormalized;

void main() {
    int xPosition = (inPosition.x == 0 ? bounds.x : bounds.z);
    int yPosition = (inPosition.y == 0 ? bounds.y : bounds.w);

    ivec2 clippedBoundPosition = ivec2(clamp(xPosition, clip.x, clip.z),clamp(yPosition, clip.y, clip.w));
    
    ivec2 texCoordUnnorm = ivec2(clippedBoundPosition.x - bounds.x, clippedBoundPosition.y - bounds.y);

    vec2 texCoord = vec2((float(texCoordUnnorm.x)/float(bounds.z - bounds.x)), (float(texCoordUnnorm.y)/float(bounds.w - bounds.y)));

    gl_Position = vec4((vec2(clippedBoundPosition) / vec2(push.renderTargetSize) * 2.0)-vec2(1,1), gl_InstanceIndex * DEPTH_INCREMENT, 1.0);

    outFlags = flags;
    outTextureIndex = index;
    outMaskIndex = maskIndex;
    outRemapPalette = remapPalette;
    outTexCoord = texCoord;
    outTexCoordUnnormalized = vec2(texCoordUnnorm);
}
