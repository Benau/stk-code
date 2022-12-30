#include "constants.h"
layout(binding = 0) uniform sampler2D f_mesh_texture_0[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 1) uniform sampler2D f_mesh_texture_1[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 2) uniform sampler2D f_mesh_texture_2[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 3) uniform sampler2D f_mesh_texture_3[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 4) uniform sampler2D f_mesh_texture_4[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 5) uniform sampler2D f_mesh_texture_5[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 6) uniform sampler2D f_mesh_texture_6[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];
layout(binding = 7) uniform sampler2D f_mesh_texture_7[SAMPLER_SIZ * TOTAL_MESH_TEXTURE_LAYE];

vec4 sampleMeshTexture0(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 0;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_0[0], uv);
}

vec4 sampleMeshTexture1(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 1;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_1[0], uv);
}

vec4 sampleMeshTexture2(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 2;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_2[0], uv);
}

vec4 sampleMeshTexture3(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 3;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_3[0], uv);
}

vec4 sampleMeshTexture4(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 4;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_4[0], uv);
}

vec4 sampleMeshTexture5(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 5;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_5[0], uv);
}

vec4 sampleMeshTexture6(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 6;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_6[0], uv);
}

vec4 sampleMeshTexture7(int material_id, vec2 uv)
{
    if (BIND_MESH_TEXTURES_AT_ONC)
    {
        int id = (TOTAL_MESH_TEXTURE_LAYE * material_id) + 7;
        return texture(f_mesh_texture_0[GE_SAMPLE_TEX_INDEX(id)], uv);
    }
    else
        return texture(f_mesh_texture_7[0], uv);
}
