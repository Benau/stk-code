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

#include "graphics/sp/sp_mesh_node.hpp"
#include "graphics/sp/sp_base.hpp"
#include "graphics/sp/sp_mesh.hpp"
#include "graphics/sp/sp_animation.hpp"
#include "graphics/sp/sp_mesh_buffer.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/material.hpp"

#include "../../lib/irrlicht/source/Irrlicht/CBoneSceneNode.h"
#include <algorithm>

namespace SP
{
// ----------------------------------------------------------------------------
SPMeshNode::SPMeshNode(IAnimatedMesh* mesh, ISceneNode* parent,
                       ISceneManager* mgr, s32 id,
                       const std::string& debug_name,
                       const core::vector3df& position,
                       const core::vector3df& rotation,
                       const core::vector3df& scale, RenderInfo* render_info)
          : CAnimatedMeshSceneNode(mesh, parent, mgr, id, position, rotation,
                                   scale)
{
    m_mesh = NULL;
    m_mesh_render_info = render_info;
    m_animated = !static_cast<SPMesh*>(mesh)->isStatic();
    m_skinning_offset = -1;
}   // SPMeshNode

// ----------------------------------------------------------------------------
SPMeshNode::~SPMeshNode()
{
    cleanJoints();
}   // ~SPMeshNode

// ----------------------------------------------------------------------------
void SPMeshNode::setAnimationState(bool val)
{
    if (!m_mesh)
    {
        return;
    }
    m_animated = !m_mesh->isStatic() && val;
}   // setAnimationState

// ----------------------------------------------------------------------------
void SPMeshNode::setMesh(irr::scene::IAnimatedMesh* mesh)
{
    m_mesh = static_cast<SPMesh*>(mesh);
    CAnimatedMeshSceneNode::setMesh(mesh);
    cleanJoints();
    if (!m_mesh->isStatic())
    {
        unsigned bone_idx = 0;
        m_skinning_matrices.resize(m_mesh->getJointCount());
        for (Armature& arm : m_mesh->getArmatures())
        {
            for (const std::string& bone_name : arm.m_joint_names)
            {
                m_joint_nodes[bone_name] = new CBoneSceneNode(this,
                    irr_driver->getSceneManager(), 0, bone_idx++,
                    bone_name.c_str());
            }
        }
    }
}   // setMesh

// ----------------------------------------------------------------------------
IBoneSceneNode* SPMeshNode::getJointNode(const c8* joint_name)
{
    auto ret = m_joint_nodes.find(joint_name);
    if (ret != m_joint_nodes.end())
    {
        return ret->second;
    }
    return NULL;
}   // getJointNode

// ----------------------------------------------------------------------------
void SPMeshNode::OnAnimate(u32 time_ms)
{
    if (m_mesh->isStatic() || !m_animated)
    {
        IAnimatedMeshSceneNode::OnAnimate(time_ms);
        return;
    }
    CAnimatedMeshSceneNode::OnAnimate(time_ms);
}   // OnAnimate

// ----------------------------------------------------------------------------
IMesh* SPMeshNode::getMeshForCurrentFrame(SkinningCallback sc, int offset)
{
    if (m_mesh->isStatic() || !m_animated)
    {
        return m_mesh;
    }
    m_mesh->getSkinningMatrices(getFrameNr(), m_skinning_matrices.data());
    updateAbsolutePosition();

    for (Armature& arm : m_mesh->getArmatures())
    {
        for (unsigned i = 0; i < arm.m_joint_names.size(); i++)
        {
            m_joint_nodes.at(arm.m_joint_names[i])->setAbsoluteTransformation
                (AbsoluteTransformation * arm.m_world_matrices[i].first);
        }
    }
    return m_mesh;
}   // getMeshForCurrentFrame

// ----------------------------------------------------------------------------
SPShader* SPMeshNode::getShader(unsigned mesh_buffer_id) const
{
    //return SP::getSPShader("solid");
    if (!m_mesh || mesh_buffer_id < m_mesh->getMeshBufferCount())
    {
        const std::string sn = (m_shader_override.empty() ?
            m_mesh->getSPMeshBuffer(mesh_buffer_id)->getSTKMaterial()
            ->getShaderName() : m_shader_override);// +
            //(m_animated ? "_skinned" : "");
        return SP::getSPShader(sn);
    }
    return NULL;
}   // getShader

}
