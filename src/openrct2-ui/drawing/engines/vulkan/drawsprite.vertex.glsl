#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in uint index;

layout(location = 0) out vec3 fragColor;

void main() {
    //if (gl_VertexIndex == 0) {
    //    debugPrintfEXT("v");
    //}
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);

    // temporary
    fragColor = index == 0 ? vec3(0.0,0.0,0.0) : vec3(1.0,1.0,1.0);
}
