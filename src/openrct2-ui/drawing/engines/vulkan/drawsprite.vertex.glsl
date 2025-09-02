#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 transform;
    uvec2 renderTargetSize;
} ubo;

layout(location = 0) in ivec4 bounds;
layout(location = 1) in ivec4 clip;
layout(location = 2) in uint flags;
layout(location = 3) in uint index;
layout(location = 4) in uint maskIndex;
layout(location = 5) in ivec2 inPosition;


layout(location = 0) flat out uint outTextureIndex;
layout(location = 1) flat out uint outFlags;
layout(location = 2) flat out uint outMaskIndex;
layout(location = 3) out vec2 outTexCoord;

void main() {
    //if (gl_VertexIndex == 0) {
    //    debugPrintfEXT("v");
    //}

    int xPosition = (inPosition.x == 0 ? bounds.x : bounds.z);
    int yPosition = (inPosition.y == 0 ? bounds.y : bounds.w);

    ivec2 clippedBoundPosition = ivec2(clamp(xPosition, clip.x, clip.z),clamp(yPosition, clip.y, clip.w));

    vec2 texCoord = vec2((float(clippedBoundPosition.x - bounds.x)/float(bounds.z - bounds.x)), (float(clippedBoundPosition.y - bounds.y)/float(bounds.w - bounds.y)));

    gl_Position = ubo.transform * vec4(vec2(clippedBoundPosition), 0.0, 1.0);

    outFlags = flags;
    outTextureIndex = index;
    outTexCoord = texCoord;
    outMaskIndex = maskIndex;
}
