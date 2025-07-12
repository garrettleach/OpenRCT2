#version 450
#pragma shader_stage(fragment)

layout(location = 0) flat in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
