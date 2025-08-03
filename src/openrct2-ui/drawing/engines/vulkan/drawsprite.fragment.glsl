#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(fragment)

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    //debugPrintfEXT("f");
    outColor = vec4(fragColor, 0.5);
}
