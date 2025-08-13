#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in uint index;
layout(location = 2) in vec2 texCoord;

layout(location = 0) flat out uint outTextureIndex;
layout(location = 1) out vec2 outTexCoord;

void main() {
    //if (gl_VertexIndex == 0) {
    //    debugPrintfEXT("v");
    //}
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);

    outTextureIndex = index;
    outTexCoord = texCoord;
}
