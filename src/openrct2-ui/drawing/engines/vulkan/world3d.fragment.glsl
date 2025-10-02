#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler singleSampler;

layout(location = 0) in vec4 colour;

layout(location = 1) out vec4 outColour;

void main() {
	outColour = colour;
}
