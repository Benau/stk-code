//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2017 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_SP_INSTANCED_DATA_HPP
#define HEADER_SP_INSTANCED_DATA_HPP

#include "utils/mini_glm.hpp"
#include "utils/vec3.hpp"
#include <cassert>
#include <matrix4.h>

using namespace irr;

namespace SP
{

class SPInstancedData
{
private:
    char m_data[40];

public:
    // ------------------------------------------------------------------------
    SPInstancedData(const core::matrix4& model_mat,
                    float texture_trans_x, float texture_trans_y, float hue,
                    float min_sat, int skinning_offset)
    {
        using namespace MiniGLM;
        float position[3] = { model_mat[12], model_mat[13], model_mat[14] };
        core::quaternion rotation(0.0f, 0.0f, 0.0f, 1.0f);
        core::vector3df scale = model_mat.getScale();
        if (scale.X != 0.0f && scale.Y != 0.0f && scale.Z != 0.0f)
        {
            core::matrix4 local_mat = model_mat;
            local_mat[0] = local_mat[0] / scale.X / local_mat[15];
            local_mat[1] = local_mat[1] / scale.X / local_mat[15];
            local_mat[2] = local_mat[2] / scale.X / local_mat[15];
            local_mat[4] = local_mat[4] / scale.Y / local_mat[15];
            local_mat[5] = local_mat[5] / scale.Y / local_mat[15];
            local_mat[6] = local_mat[6] / scale.Y / local_mat[15];
            local_mat[8] = local_mat[8] / scale.Z / local_mat[15];
            local_mat[9] = local_mat[9] / scale.Z / local_mat[15];
            local_mat[10] = local_mat[10] / scale.Z / local_mat[15];
            rotation = getQuaternion(local_mat);
            // Conjugated quaternion in glsl
            rotation.W = -rotation.W;
        }
        memcpy(m_data, position, 12);
        short q[4] = { toFloat16(rotation.X), toFloat16(rotation.Y),
            toFloat16(rotation.Z), toFloat16(rotation.W)};
        memcpy(m_data + 12, q, 8);
        short s[4] = { toFloat16(scale.X), toFloat16(scale.Y),
            toFloat16(scale.Z), 0};
        memcpy(m_data + 20, s, 8);
        short md[4] = { toFloat16(texture_trans_x), toFloat16(texture_trans_y),
            toFloat16(hue), toFloat16(min_sat)};
        memcpy(m_data + 28, md, 8);
        memcpy(m_data + 36, &skinning_offset, 4);
    }
    // ------------------------------------------------------------------------
    const void* getData() const { return m_data; }
};



}

#endif
