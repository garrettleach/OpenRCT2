#version 450
//#extension GL_EXT_debug_printf : enable
#pragma shader_stage(fragment)
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in flat uint colour;

layout(location = 0) out uint outColour;

void main() {
    outColour = colour;
}
