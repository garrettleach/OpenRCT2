#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(vertex)

layout(push_constant) uniform pushConstants
{
	mat4 transform;
    uvec2 renderTargetSize;
	uint landHeightStep;
} push;

layout(location = 0) in ivec2 squarePos;
layout(location = 1) in uint squareHeight;
layout(location = 2) in	uint cornerHeights;
layout(location = 3) in vec2 vertexPos;

layout(location = 0) out vec4 outColour;

void main() {
	uint cornerHeight = (cornerHeights>>(gl_VertexIndex<<1) & 0x3) * push.landHeightStep;

	gl_Position = push.transform * vec4(vec3((vec2(squarePos)+vertexPos), float(squareHeight + cornerHeight)/push.landHeightStep),1.0);

	outColour = vec4(vertexPos.x,vertexPos.y,float(squareHeight)/255.0f,1.0f);
}
