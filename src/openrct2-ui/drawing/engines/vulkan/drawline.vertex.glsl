#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

// Allows for about 8 million draws per frame
const float DEPTH_INCREMENT = 1.0 / float(1u << 22u);//1.0 / float(1u << 22u);

layout(push_constant) uniform PushConstants {
    uvec2 renderTargetSize;
} pushConstants;

layout(location = 0) in ivec2 pos;
layout(location = 1) in uint depth;
layout(location = 2) in uint colour;
//layout(location = 3) in vec4 bounds; // TODO: do we need to do a clip/bounds check?

layout(location = 0) out flat uint outColour;

void main()
{
	gl_Position = vec4((vec2(pos) / vec2(pushConstants.renderTargetSize) * 2) - vec2(1.0,1.0), depth * DEPTH_INCREMENT, 1.0);

	outColour = colour;
}
