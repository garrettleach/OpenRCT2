#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 1) uniform sampler singleSampler;
layout(set = 0, binding = 2) uniform DrawInfo
{
    vec4 palette[256];
} drawInfo;

layout(set = 1, binding = 0) uniform texture2D textures[];

layout(location = 0) flat in uint textureIndex;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

void main() {
    //debugPrintfEXT("f");

    outColor = texture(sampler2D(textures[textureIndex], singleSampler), texCoord);
}
