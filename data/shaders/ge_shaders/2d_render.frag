#include "utils/constants.h"

layout(location = 0) in vec4 f_color;
layout(location = 1) in vec2 f_uv;
layout(location = 2) flat in int f_sampler_index;

layout(binding = 0) uniform sampler2D f_tex[SAMPLER_SIZE];

layout(location = 0) out vec4 o_color;

void main()
{
    vec4 tex_color = vec4(0.0);
    if (BIND_TEXTURES_AT_ONC)
        tex_color = texture(f_tex[GE_SAMPLE_TEX_INDEX(f_sampler_index)], f_uv);
    else
        tex_color = texture(f_tex[0], f_uv);
    o_color = tex_color * f_color;
}
