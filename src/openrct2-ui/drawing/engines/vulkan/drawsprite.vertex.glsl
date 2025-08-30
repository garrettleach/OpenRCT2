#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 transform;
    uvec2 renderTargetSize;
} ubo;

layout(location = 0) in vec4 clip;
layout(location = 1) in uint flags;
layout(location = 2) in uint index;
layout(location = 3) in uint maskIndex;
layout(location = 4) in vec2 inPosition;
layout(location = 5) in vec2 texCoord;


layout(location = 0) flat out uint outTextureIndex;
layout(location = 1) flat out uint outFlags;
layout(location = 2) flat out uint outMaskIndex;
layout(location = 3) out vec2 outTexCoord;

void main() {
    //if (gl_VertexIndex == 0) {
    //    debugPrintfEXT("v");
    //}
    gl_Position = ubo.transform * vec4(inPosition, 0.0, 1.0);

    outFlags = flags;
    outTextureIndex = index;
    outTexCoord = texCoord;
    outMaskIndex = maskIndex;
}
